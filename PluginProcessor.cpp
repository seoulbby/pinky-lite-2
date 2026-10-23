// Author: seoulbby
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Shapers.h"

using namespace juce;

static constexpr float kMaxDriveDb = 42.0f;
static constexpr float kAutoGainSlope = 0.38f;

PinkyProcessor::PinkyProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PINKY", pinky::createParameterLayout())
{
    using namespace pinky::id;
    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pInGain = P (inGain); pDrive = P (drive); pMode = P (mode); pShape = P (shape); pBias = P (bias); pTone = P (tone);
    pXover = P (xover); pLow = P (lowAmt); pHigh = P (highAmt); pQuality = P (pinky::id::quality);
    pBits = P (bits); pRate = P (rate); pFType = P (fType); pCutoff = P (cutoff); pReso = P (reso);
    pWidth = P (width); pMix = P (mix); pOut = P (outGain);
    pLfoRate = P (lfoRate); pLfoSync = P (lfoSync); pLfoShape = P (lfoShape);
    pLfoDrive = P (lfoDrive); pLfoCut = P (lfoCut); pLfoCrush = P (lfoCrush);

    for (int q = 0; q < kNumQualities; ++q)
        oversampling[q] = std::make_unique<dsp::Oversampling<float>> (2, (size_t) q,
            dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, true);

    for (auto* v : { &lfoBuf, &driveBuf, &lowBuf, &highBuf, &shapeBuf, &biasBuf, &xoverBuf })
        v->assign ((size_t) kMaxBlock, 0.0f);
}

bool PinkyProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto o = l.getMainOutputChannelSet();
    return (o == AudioChannelSet::stereo() || o == AudioChannelSet::mono()) && o == l.getMainInputChannelSet();
}

void PinkyProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    const int numCh = jmax (1, getTotalNumInputChannels());

    for (int q = 0; q < kNumQualities; ++q)
    {
        oversampling[q]->initProcessing ((size_t) kMaxBlock);
        oversampling[q]->reset();
        dsp::ProcessSpec s { sr * (double) (1 << q), (uint32) kMaxBlock * (uint32) (1 << q), 2 };
        crossover[q].prepare (s);
        crossover[q].setType (dsp::LinkwitzRileyFilterType::lowpass);
        crossover[q].reset();
    }

    dsp::ProcessSpec spec { sr, (uint32) kMaxBlock, 2 };
    dryAllpass.prepare (spec);
    dryAllpass.setType (dsp::LinkwitzRileyFilterType::lowpass);
    dryAllpass.reset();
    for (auto& f : filter) { f.prepare (spec); f.reset(); }
    dryDelay.setMaximumDelayInSamples (512);
    dryDelay.prepare (spec);
    dryDelay.reset();

    const double ramp = 0.03;
    auto init = [&] (auto& s, float v, double r = 0.03) { s.reset (sr, r); s.setCurrentAndTargetValue (v); };
    init (sIn, 1.0f); init (sOut, 1.0f); init (sMix, 1.0f); init (sWidth, 1.0f); init (sTone, 0.0f);
    init (sDrive, 0.3f); init (sShape, 0.5f); init (sBias, 0.0f); init (sLow, 1.0f); init (sHigh, 1.0f);
    init (sBits, 16.0f); init (sRate, 1.0f); init (sCutoff, 8000.0f, ramp); init (sXover, 700.0f, ramp);

    for (int c = 0; c < 2; ++c) { dcX[c] = dcY[c] = toneLp[c] = held[c] = 0.0f; crushPhase[c] = 1.0f; }
    lfoSmooth = 0.0f; lastFilterType = -1;
    (void) numCh;

    quality = -1;
    selectQuality ((int) pQuality->load());
}

void PinkyProcessor::selectQuality (int q)
{
    q = jlimit (0, kNumQualities - 1, q);
    if (q == quality) return;
    quality = q;
    oversampling[q]->reset();
    crossover[q].reset();
    const float lat = oversampling[q]->getLatencyInSamples();
    const int latInt = roundToInt (lat);
    setLatencySamples (latInt);
    dryDelay.setDelay ((float) latInt);
    dryDelay.reset();
}

void PinkyProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int numCh = jmin (2, buffer.getNumChannels());
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, total);
    if (numCh == 0 || total == 0) return;

    selectQuality ((int) pQuality->load());

    for (int pos = 0; pos < total; pos += kMaxBlock)
    {
        const int len = jmin (kMaxBlock, total - pos);
        float* ptrs[2] = { buffer.getWritePointer (0) + pos, buffer.getWritePointer (numCh > 1 ? 1 : 0) + pos };
        AudioBuffer<float> chunk (ptrs, numCh, len);

        // keep the LFO locked to the host timeline from the start of every chunk
        processChunk (chunk);
    }
}

void PinkyProcessor::processChunk (AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    const float sr = (float) sampleRate;
    const bool autoGain = settings.autoGain();
    const bool safety = settings.safety();

    // ---------------------------------------------------------------- targets
    sIn.setTargetValue (Decibels::decibelsToGain (pInGain->load()));
    sOut.setTargetValue (Decibels::decibelsToGain (pOut->load()));
    sMix.setTargetValue (pMix->load() * 0.01f);
    sWidth.setTargetValue (pWidth->load() * 0.01f);
    sTone.setTargetValue (pTone->load() * 0.01f);
    sDrive.setTargetValue (pDrive->load() * 0.01f);
    sShape.setTargetValue (pShape->load() * 0.01f);
    sBias.setTargetValue (pBias->load() * 0.01f);
    sLow.setTargetValue (pLow->load() * 0.01f);
    sHigh.setTargetValue (pHigh->load() * 0.01f);
    sBits.setTargetValue (pBits->load());
    sRate.setTargetValue (pRate->load());
    sCutoff.setTargetValue (jlimit (20.0f, 20000.0f, pCutoff->load()));
    sXover.setTargetValue (jlimit (40.0f, 12000.0f, pXover->load()));

    const int mode = (int) pMode->load();
    const int fType = (int) pFType->load();
    const float reso = pReso->load() * 0.01f;
    const int lfoShape = (int) pLfoShape->load();
    const int sync = (int) pLfoSync->load();
    const float dDrive = pLfoDrive->load() * 0.01f;
    const float dCut = pLfoCut->load() * 0.01f;
    const float dCrush = pLfoCrush->load() * 0.01f;

    // ---------------------------------------------------------------- input gain, meter, dry path
    float inPk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = sIn.getNextValue();
        for (int c = 0; c < numCh; ++c)
        {
            float* d = buffer.getWritePointer (c);
            d[i] *= g;
            inPk = jmax (inPk, std::abs (d[i]));
            dryDelay.pushSample (c, d[i]);
            dryBuf.setSample (c, i, dryDelay.popSample (c));
        }
    }

    // ---------------------------------------------------------------- LFO + control signals
    double bpm = 120.0;
    bool locked = false;
    double ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition()) { ppq = *p; locked = true; }
        }

    double inc;
    if (sync > 0)
    {
        const double beats = pinky::syncBeats (sync);
        inc = (bpm / 60.0) / ((double) sr * beats);
        if (locked) lfoPhase = std::fmod (ppq / beats, 1.0);
    }
    else
        inc = (double) pLfoRate->load() / (double) sr;

    const float smoothCoef = 1.0f - std::exp (-1.0f / (0.0015f * sr));
    const float randCoef = 1.0f - std::exp (-1.0f / (0.02f * sr));

    for (int i = 0; i < n; ++i)
    {
        lfoPhase += inc;
        if (lfoPhase >= 1.0) lfoPhase -= std::floor (lfoPhase);
        if (lfoPhase < prevLfoPhase) randTarget = random.nextFloat() * 2.0f - 1.0f;
        prevLfoPhase = lfoPhase;

        float raw;
        if (lfoShape == pinky::Random) { randValue += (randTarget - randValue) * randCoef; raw = randValue; }
        else raw = pinky::lfoEval (lfoShape, (float) lfoPhase);

        lfoSmooth += (raw - lfoSmooth) * smoothCoef;
        lfoBuf[(size_t) i] = lfoSmooth;

        const float d = jlimit (0.0f, 1.0f, sDrive.getNextValue() + lfoSmooth * dDrive * 0.5f);
        driveBuf[(size_t) i] = d;
        lowBuf[(size_t) i] = sLow.getNextValue();
        highBuf[(size_t) i] = sHigh.getNextValue();
        shapeBuf[(size_t) i] = sShape.getNextValue();
        biasBuf[(size_t) i] = sBias.getNextValue();
        xoverBuf[(size_t) i] = sXover.getNextValue();
    }
    // dry path gets the same crossover all-pass phase as the wet path, so Mix blends stay coherent
    for (int c = 0; c < numCh; ++c)
    {
        float* d = dryBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0) dryAllpass.setCutoffFrequency (xoverBuf[(size_t) i]);
            float lo, hi;
            dryAllpass.processSample (c, d[i], lo, hi);
            d[i] = lo + hi;
        }
    }
    if (numCh == 1) dryBuf.copyFrom (1, 0, dryBuf, 0, 0, n);

    liveDrive = driveBuf[(size_t) (n - 1)];
    lfoPhaseOut = (float) lfoPhase;
    lfoValueOut = lfoSmooth;

    // ---------------------------------------------------------------- distortion (oversampled, 2 bands)
    {
        dsp::AudioBlock<float> block (buffer);
        auto& os = *oversampling[quality];
        auto up = os.processSamplesUp (block);
        const int factor = (int) os.getOversamplingFactor();
        const int upN = (int) up.getNumSamples();
        auto& xo = crossover[quality];

        for (int c = 0; c < numCh; ++c)
        {
            float* d = up.getChannelPointer ((size_t) c);
            for (int j = 0; j < upN; ++j)
            {
                const size_t i = (size_t) (j / factor);
                if ((j & 15) == 0) xo.setCutoffFrequency (xoverBuf[i]);

                float lo, hi;
                xo.processSample (c, d[j], lo, hi);

                const float dn = driveBuf[i], sh = shapeBuf[i], bi = biasBuf[i];
                const float dbL = kMaxDriveDb * jmin (1.5f, dn * lowBuf[i]);
                const float dbH = kMaxDriveDb * jmin (1.5f, dn * highBuf[i]);
                const float gL = Decibels::decibelsToGain (dbL), gH = Decibels::decibelsToGain (dbH);
                float yL = pinky::shapeSample (mode, lo * gL, sh, bi);
                float yH = pinky::shapeSample (mode, hi * gH, sh, bi);
                if (autoGain)
                {
                    yL *= Decibels::decibelsToGain (-kAutoGainSlope * dbL);
                    yH *= Decibels::decibelsToGain (-kAutoGainSlope * dbH);
                }
                d[j] = yL + yH;
            }
        }
        os.processSamplesDown (block);
    }

    // ---------------------------------------------------------------- post: dc, crush, filter, tone, width, mix
    const float dcR = 1.0f - 6.2831853f * 8.0f / sr;
    const float toneA = 1.0f - std::exp (-6.2831853f * 1000.0f / sr);
    const float q = 0.707f + reso * reso * 14.0f;
    const bool filterOn = fType > 0;

    if (filterOn)
        for (auto& f : filter)
        {
            f.setType (fType == 1 ? dsp::StateVariableTPTFilterType::lowpass
                     : fType == 2 ? dsp::StateVariableTPTFilterType::highpass
                                  : dsp::StateVariableTPTFilterType::bandpass);
            f.setResonance (q);
        }
    if (fType != lastFilterType) { for (auto& f : filter) f.reset(); lastFilterType = fType; }

    float outPk = 0.0f, cutNow = sCutoff.getCurrentValue();
    for (int i = 0; i < n; ++i)
    {
        const float lfo = lfoBuf[(size_t) i];
        const float bits = jlimit (1.0f, 16.0f, sBits.getNextValue() + lfo * dCrush * 7.0f);
        const float rate = jmax (1.0f, sRate.getNextValue() * std::exp2 (lfo * dCrush * 1.5f));
        cutNow = jlimit (20.0f, 20000.0f, sCutoff.getNextValue() * std::exp2 (lfo * dCut * 3.5f));
        if (filterOn && (i & 15) == 0) for (auto& f : filter) f.setCutoffFrequency (jmin (cutNow, sr * 0.45f));

        const float tone = sTone.getNextValue();
        const float width = sWidth.getNextValue();
        const float mix = sMix.getNextValue();
        const float outG = sOut.getNextValue();
        const float toneG = std::exp (tone * 0.92f);
        const float wetG = std::sin (mix * 1.5707963f), dryG = std::cos (mix * 1.5707963f);

        float s[2] = { 0.0f, 0.0f };
        for (int c = 0; c < numCh; ++c)
        {
            float x = buffer.getSample (c, i);
            if (! std::isfinite (x)) x = 0.0f;

            // DC blocker
            const float y0 = x - dcX[c] + dcR * dcY[c];
            dcX[c] = x; dcY[c] = y0; x = y0;

            // crush: sample-rate reduction then bit reduction
            if (rate > 1.001f)
            {
                crushPhase[c] += 1.0f / rate;
                if (crushPhase[c] >= 1.0f) { crushPhase[c] -= 1.0f; held[c] = x; }
                x = held[c];
            }
            else { held[c] = x; crushPhase[c] = 1.0f; }
            if (bits < 15.95f)
            {
                const float lv = std::exp2 (bits - 1.0f);
                x = std::round (x * lv) / lv;
            }

            if (filterOn) x = filter[c].processSample (c, x);

            // tilt tone
            toneLp[c] += toneA * (x - toneLp[c]);
            x = toneLp[c] * toneG + (x - toneLp[c]) / toneG;

            s[c] = x;
        }

        if (numCh == 2)
        {
            const float m = 0.5f * (s[0] + s[1]) , sd = 0.5f * (s[0] - s[1]) * width;
            s[0] = m + sd; s[1] = m - sd;
        }
        else s[1] = s[0];

        for (int c = 0; c < numCh; ++c)
        {
            float out = (dryBuf.getSample (c, i) * dryG + s[c] * wetG) * outG;
            if (safety)
            {
                const float a = std::abs (out);
                if (a > 0.95f) out = std::copysign (0.95f + 0.05f * std::tanh ((a - 0.95f) / 0.05f), out);
            }
            if (! std::isfinite (out)) out = 0.0f;
            buffer.setSample (c, i, out);
            outPk = jmax (outPk, std::abs (out));
        }
    }
    liveCutoff = cutNow;

    // meters with decay
    const float decay = std::pow (0.9992f, (float) n);
    peakInDecay = jmax (inPk, peakInDecay * decay);
    peakOutDecay = jmax (outPk, peakOutDecay * decay);
    inPeak = peakInDecay;
    outPeak = peakOutDecay;
}

// ------------------------------------------------------------------ programs & state
int PinkyProcessor::getNumPrograms() { return (int) pinky::factoryPresets().size(); }

const String PinkyProcessor::getProgramName (int i)
{
    auto& p = pinky::factoryPresets();
    return isPositiveAndBelow (i, (int) p.size()) ? String (p[(size_t) i].name) : String();
}

void PinkyProcessor::setCurrentProgram (int idx)
{
    auto& presets = pinky::factoryPresets();
    if (! isPositiveAndBelow (idx, (int) presets.size())) return;
    currentProgram = idx;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            if (rp->paramID != pinky::id::quality)
                rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& kv : presets[(size_t) idx].values)
        if (auto* rp = apvts.getParameter (kv.first))
            rp->setValueNotifyingHost (rp->convertTo0to1 (kv.second));
}

void PinkyProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.appendChild (settings.tree.createCopy(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PinkyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            auto sc = state.getChildWithName ("SETTINGS");
            if (sc.isValid())
            {
                settings.loadFrom (sc);
                state.removeChild (sc, nullptr);
            }
            currentProgram = jmax (0, (int) state.getProperty ("program", 0));
            apvts.replaceState (state);
        }
}

AudioProcessorEditor* PinkyProcessor::createEditor() { return new PinkyEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PinkyProcessor(); }
