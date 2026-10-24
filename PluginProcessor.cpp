// Author: seoulbby
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Lfo.h"

using namespace juce;

PinkyProcessor::PinkyProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PINKY", pinky::createParameterLayout())
{
    lfoBuf.assign ((size_t) kMaxBlock, 0.0f);
}

bool PinkyProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto o = l.getMainOutputChannelSet();
    return (o == AudioChannelSet::stereo() || o == AudioChannelSet::mono()) && o == l.getMainInputChannelSet();
}

void PinkyProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    wobble.prepare (sr);
    glitch.prepare (sr);
    glass.prepare (sr);
    shimmer.prepare (sr);

    dsp::ProcessSpec spec { sr, (uint32) kMaxBlock, 2 };
    for (auto& f : filter) { f.prepare (spec); f.reset(); }

    auto init = [&] (auto& s, float v, double r = 0.03) { s.reset (sr, r); s.setCurrentAndTargetValue (v); };
    using namespace pinky::id;
    init (sIn, Decibels::decibelsToGain (rawParam (inGain))); init (sOut, Decibels::decibelsToGain (rawParam (outGain)));
    init (sMix, rawParam (mix) * 0.01f); init (sWidth, rawParam (width) * 0.01f); init (sTone, rawParam (tone) * 0.01f);
    init (sWob, rawParam (wobDepth) * 0.01f); init (sShim, rawParam (shimAmt) * 0.01f); init (sGlass, rawParam (glassAmt) * 0.01f);
    init (sGl, rawParam (glAmt) * 0.01f); init (sBits, rawParam (bits)); init (sRate, rawParam (rate));
    init (sWobRate, rawParam (wobRate)); init (sCutoff, jlimit (20.0f, 20000.0f, rawParam (cutoff)));

    for (int c = 0; c < 2; ++c) { toneLp[c] = held[c] = 0.0f; crushPhase[c] = 1.0f; }
    lfoSmooth = 0.0f; lastFilterType = -1; lastKey = lastScale = -1; lastSpread = lastGlassDecay = lastShimDecay = -1.0f; lastShimPitch = -99.0f;
}

void PinkyProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int numCh = jmin (2, buffer.getNumChannels());
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, total);
    if (numCh == 0 || total == 0) return;

    for (int pos = 0; pos < total; pos += kMaxBlock)
    {
        const int len = jmin (kMaxBlock, total - pos);
        float* ptrs[2] = { buffer.getWritePointer (0) + pos, buffer.getWritePointer (numCh > 1 ? 1 : 0) + pos };
        AudioBuffer<float> chunk (ptrs, numCh, len);
        processChunk (chunk, pos);
    }
}

void PinkyProcessor::processChunk (AudioBuffer<float>& buffer, int offsetSamples)
{
    using namespace pinky::id;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    const float sr = (float) sampleRate;
    const bool safety = settings.safety();

    // ---------------------------------------------------------------- targets
    sIn.setTargetValue (Decibels::decibelsToGain (rawParam (inGain)));
    sOut.setTargetValue (Decibels::decibelsToGain (rawParam (outGain)));
    sMix.setTargetValue (rawParam (mix) * 0.01f);
    sWidth.setTargetValue (rawParam (width) * 0.01f);
    sTone.setTargetValue (rawParam (tone) * 0.01f);
    sWob.setTargetValue (rawParam (wobDepth) * 0.01f);
    sWobRate.setTargetValue (rawParam (wobRate));
    sShim.setTargetValue (rawParam (shimAmt) * 0.01f);
    sGlass.setTargetValue (rawParam (glassAmt) * 0.01f);
    sGl.setTargetValue (rawParam (glAmt) * 0.01f);
    sBits.setTargetValue (rawParam (bits));
    sRate.setTargetValue (rawParam (rate));
    sCutoff.setTargetValue (jlimit (20.0f, 20000.0f, rawParam (cutoff)));

    const float wobSpreadV = rawParam (wobSpread) * 0.01f;
    const int fType = (int) rawParam (pinky::id::fType);
    const float resoV = rawParam (reso) * 0.01f;
    const int lfoShapeV = (int) rawParam (lfoShape);
    const int sync = (int) rawParam (lfoSync);
    const float dWob = rawParam (lfoWob) * 0.01f, dShim = rawParam (lfoShim) * 0.01f;
    const float dGlass = rawParam (lfoGlass) * 0.01f, dCut = rawParam (lfoCut) * 0.01f;

    // retune the musical modules only when something changed
    const int key = (int) rawParam (glassKey), scale = (int) rawParam (glassScale);
    const float spread = rawParam (glassSpread) * 0.01f, gDecay = rawParam (glassDecay) * 0.01f;
    if (key != lastKey || scale != lastScale || spread != lastSpread)
    { glass.setTuning (key, scale, spread); lastKey = key; lastScale = scale; lastSpread = spread; lastGlassDecay = -1.0f; }
    if (gDecay != lastGlassDecay) { glass.setDecay (gDecay); lastGlassDecay = gDecay; }
    const float sDecayV = rawParam (shimDecay) * 0.01f, sPitch = rawParam (shimPitch);
    if (sDecayV != lastShimDecay) { shimmer.setDecay (sDecayV); lastShimDecay = sDecayV; }
    if (sPitch != lastShimPitch) { shimmer.setPitch (sPitch); lastShimPitch = sPitch; }

    // ---------------------------------------------------------------- host tempo
    double bpm = 120.0;
    bool locked = false;
    double ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = jmax (20.0, *b);
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition()) { ppq = *p + (double) offsetSamples * (bpm / 60.0) / sampleRate; locked = true; }
        }

    glitch.setTiming (bpm, pinky::glitchDivBeats ((int) rawParam (glDiv)), locked, ppq, (int) rawParam (glType),
                      rawParam (glChance) * 0.01f, settings.repeatable());
    const float glSmoothMs = rawParam (glSmooth);

    // ---------------------------------------------------------------- LFO
    double inc;
    if (sync > 0)
    {
        const double beats = pinky::syncBeats (sync);
        inc = (bpm / 60.0) / ((double) sr * beats);
        if (locked) lfoPhase = std::fmod (ppq / beats, 1.0);
    }
    else
        inc = (double) rawParam (lfoRate) / (double) sr;

    const float smoothCoef = 1.0f - std::exp (-1.0f / (0.0015f * sr));
    const float randCoef = 1.0f - std::exp (-1.0f / (0.02f * sr));
    for (int i = 0; i < n; ++i)
    {
        lfoPhase += inc;
        if (lfoPhase >= 1.0) lfoPhase -= std::floor (lfoPhase);
        if (lfoPhase < prevLfoPhase) randTarget = random.nextFloat() * 2.0f - 1.0f;
        prevLfoPhase = lfoPhase;

        float raw;
        if (lfoShapeV == pinky::Random) { randValue += (randTarget - randValue) * randCoef; raw = randValue; }
        else raw = pinky::lfoEval (lfoShapeV, (float) lfoPhase);
        lfoSmooth += (raw - lfoSmooth) * smoothCoef;
        lfoBuf[(size_t) i] = lfoSmooth;
    }
    lfoPhaseOut = (float) lfoPhase;
    lfoValueOut = lfoSmooth;

    // ---------------------------------------------------------------- filter set-up
    const float q = 0.707f + resoV * resoV * 14.0f;
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

    const float toneA = 1.0f - std::exp (-6.2831853f * 1000.0f / sr);

    // ---------------------------------------------------------------- main loop
    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float inPk = 0.0f, outPk = 0.0f, cutNow = sCutoff.getCurrentValue();
    float wobNow = 0.0f, shimNow = 0.0f, glassNow = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float lfo = lfoBuf[(size_t) i];
        const float gIn = sIn.getNextValue();
        float l = ch0[i] * gIn;
        float r = ch1 != nullptr ? ch1[i] * gIn : l;
        inPk = jmax (inPk, std::abs (l), std::abs (r));
        const float dryL = l, dryR = r;

        // 1. glitch (rhythmic slicing of the incoming melody)
        glitch.process (l, r, sGl.getNextValue(), glSmoothMs);

        // 2. wobble (dreamy pitch drift)
        wobNow = jlimit (0.0f, 1.0f, sWob.getNextValue() + lfo * dWob * 0.5f);
        wobble.process (l, r, wobNow, sWobRate.getNextValue(), wobSpreadV);

        // 3. glass (key-tuned resonators)
        glassNow = jlimit (0.0f, 1.0f, sGlass.getNextValue() + lfo * dGlass * 0.6f);
        float gL, gR;
        glass.process (l, r, gL, gR);
        l += glassNow * gL * 1.8f;
        r += glassNow * gR * 1.8f;

        // 4. shimmer (pitch-shifted reverb tail)
        shimNow = jlimit (0.0f, 1.0f, sShim.getNextValue() + lfo * dShim * 0.6f);
        float sL, sR;
        shimmer.process (l, r, sL, sR);
        l += shimNow * sL * 1.3f;
        r += shimNow * sR * 1.3f;

        // 5. crush
        const float bitsV = jlimit (1.0f, 16.0f, sBits.getNextValue());
        const float rateV = jmax (1.0f, sRate.getNextValue());
        float s[2] = { l, r };
        for (int c = 0; c < numCh; ++c)
        {
            float x = s[c];
            if (! std::isfinite (x)) x = 0.0f;
            if (rateV > 1.001f)
            {
                crushPhase[c] += 1.0f / rateV;
                if (crushPhase[c] >= 1.0f) { crushPhase[c] -= 1.0f; held[c] = x; }
                x = held[c];
            }
            else { held[c] = x; crushPhase[c] = 1.0f; }
            if (bitsV < 15.95f)
            {
                const float lv = std::exp2 (bitsV - 1.0f);
                x = std::round (x * lv) / lv;
            }
            s[c] = x;
        }

        // 6. filter
        cutNow = jlimit (20.0f, 20000.0f, sCutoff.getNextValue() * std::exp2 (lfo * dCut * 3.5f));
        if (filterOn)
        {
            if ((i & 15) == 0) for (auto& f : filter) f.setCutoffFrequency (jmin (cutNow, sr * 0.45f));
            for (int c = 0; c < numCh; ++c) s[c] = filter[c].processSample (c, s[c]);
        }

        // 7. tone tilt, width
        const float tone = sTone.getNextValue();
        const float toneG = std::exp (tone * 0.92f);
        for (int c = 0; c < numCh; ++c)
        {
            toneLp[c] += toneA * (s[c] - toneLp[c]);
            s[c] = toneLp[c] * toneG + (s[c] - toneLp[c]) / toneG;
        }
        if (numCh == 2)
        {
            const float w = sWidth.getNextValue();
            const float m = 0.5f * (s[0] + s[1]), sd = 0.5f * (s[0] - s[1]) * w;
            s[0] = m + sd; s[1] = m - sd;
        }
        else sWidth.getNextValue();

        // 8. mix, output, safety
        const float mixV = sMix.getNextValue();
        const float wetG = std::sin (mixV * 1.5707963f), dryG = std::cos (mixV * 1.5707963f);
        const float outG = sOut.getNextValue();
        const float dry[2] = { dryL, dryR };
        for (int c = 0; c < numCh; ++c)
        {
            float out = (dry[c] * dryG + s[c] * wetG) * outG;
            if (safety)
            {
                const float a = std::abs (out);
                if (a > 0.95f) out = std::copysign (0.95f + 0.05f * std::tanh ((a - 0.95f) / 0.05f), out);
            }
            if (! std::isfinite (out)) out = 0.0f;
            (c == 0 ? ch0 : ch1)[i] = out;
            outPk = jmax (outPk, std::abs (out));
        }
    }

    liveCutoff = cutNow;
    liveWobble = wobNow; liveShimmer = shimNow; liveGlass = glassNow;
    wobbleOut = wobble.lfoValue;
    shimLevel = jmin (1.0f, shimmer.level * 2.0f);
    glassLevel = jmin (1.0f, glass.level * 4.0f);
    glitchHistory = glitch.history.load();
    glitchPos = glitch.posOut.load();

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
