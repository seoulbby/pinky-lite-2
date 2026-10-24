// Pinky Lite 2 - audio processor
// Author: seoulbby
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Settings.h"
#include "Wobble.h"
#include "Glitch.h"
#include "Glass.h"
#include "Shimmer.h"

class PinkyProcessor : public juce::AudioProcessor
{
public:
    PinkyProcessor();
    ~PinkyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    pinky::PinkySettings settings;

    // live values for the UI (written on the audio thread)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<float> liveCutoff { 8000.0f };    // Hz after LFO
    std::atomic<float> lfoPhaseOut { 0.0f };      // 0..1
    std::atomic<float> lfoValueOut { 0.0f };      // -1..1
    std::atomic<float> wobbleOut { 0.0f };        // -1..1
    std::atomic<float> shimLevel { 0.0f }, glassLevel { 0.0f };
    std::atomic<float> liveWobble { 0.0f }, liveShimmer { 0.0f }, liveGlass { 0.0f };   // 0..1 after LFO
    std::atomic<std::uint32_t> glitchHistory { 0 };
    std::atomic<float> glitchPos { 0.0f };

private:
    static constexpr int kMaxBlock = 1024;

    void processChunk (juce::AudioBuffer<float>& buffer, int offsetSamples);
    float rawParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    double sampleRate = 44100.0;
    int currentProgram = 0;

    pinky::Wobble wobble;
    pinky::Glitch glitch;
    pinky::Glass glass;
    pinky::Shimmer shimmer;
    juce::dsp::StateVariableTPTFilter<float> filter[2];

    juce::SmoothedValue<float> sIn, sOut, sMix, sWidth, sTone, sWob, sShim, sGlass, sGl, sBits, sRate, sWobRate;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> sCutoff;

    std::vector<float> lfoBuf;

    double lfoPhase = 0.0, prevLfoPhase = 0.0;
    float lfoSmooth = 0.0f, randTarget = 0.0f, randValue = 0.0f;
    juce::Random random;

    float toneLp[2] {}, held[2] {}, crushPhase[2] {};
    float peakInDecay = 0.0f, peakOutDecay = 0.0f;
    int lastFilterType = -1, lastKey = -1, lastScale = -1;
    float lastSpread = -1.0f, lastGlassDecay = -1.0f, lastShimDecay = -1.0f, lastShimPitch = -99.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PinkyProcessor)
};
