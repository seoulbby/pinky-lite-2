// Pinky Lite 2 - dreamy pitch wobble (tape wow + flutter, stereo spread)
// Author: seoulbby
#pragma once
#include "Ring.h"
#include <juce_core/juce_core.h>

namespace pinky
{
class Wobble
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const int n = Ring::pow2AtLeast (0.05 * sampleRate);
        for (auto& r : ring) r.init (n);
        noiseCoef = 1.0f - std::exp (-6.2831853f * 9.0f / sr);
        reset();
    }
    void reset()
    {
        for (auto& r : ring) r.clear();
        phase = 0.0f; flutter = 0.0f; noiseTarget = 0.0f; noiseCount = 0;
    }

    // depth 0..1, rate in Hz, spread 0..1
    void process (float& l, float& r, float depth, float rateHz, float spread) noexcept
    {
        phase += rateHz / sr;
        if (phase >= 1.0f) phase -= 1.0f;
        if (--noiseCount <= 0) { noiseCount = (int) (sr / 14.0f); noiseTarget = rng.nextFloat() * 2.0f - 1.0f; }
        flutter += (noiseTarget - flutter) * noiseCoef;

        ring[0].push (l);
        ring[1].push (r);

        const float base = 0.009f * sr, amp = 0.0055f * sr * depth;
        const float ml = 0.7f * std::sin (6.2831853f * phase) + 0.3f * flutter;
        const float mr = 0.7f * std::sin (6.2831853f * (phase + 0.5f * spread)) - 0.3f * flutter;
        lfoValue = ml;

        const float wl = ring[0].read (base + amp * ml);
        const float wr = ring[1].read (base + amp * mr);
        const float m = juce::jmin (1.0f, depth * 16.0f);   // no delay at all when depth is 0
        l += (wl - l) * m;
        r += (wr - r) * m;
    }

    float lfoValue = 0.0f;

private:
    Ring ring[2];
    float sr = 44100.0f, phase = 0.0f, flutter = 0.0f, noiseTarget = 0.0f, noiseCoef = 0.001f;
    int noiseCount = 0;
    juce::Random rng { 4242 };
};
} // namespace pinky
