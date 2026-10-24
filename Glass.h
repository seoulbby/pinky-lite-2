// Pinky Lite 2 - "Glass": a bank of comb resonators tuned to a musical key/scale.
// Notes in the melody that belong to the scale ring out like glass bells.
// Author: seoulbby
#pragma once
#include "Ring.h"

namespace pinky
{
inline constexpr const char* keyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
inline constexpr const char* scaleNames[6] = { "Major", "Minor", "Maj Pent", "Min Pent", "Lydian", "Dorian" };

inline int scaleSize (int s) { static constexpr int n[6] = { 7, 7, 5, 5, 7, 7 }; return n[s]; }
inline const int* scaleIntervals (int s)
{
    static constexpr int t[6][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 7, 9, 0, 0 },
                                     { 0, 3, 5, 7, 10, 0, 0 }, { 0, 2, 4, 6, 7, 9, 11 }, { 0, 2, 3, 5, 7, 9, 10 } };
    return t[s < 0 ? 0 : (s > 5 ? 5 : s)];
}
inline bool noteInScale (int pitchClass, int key, int scale)
{
    const int rel = ((pitchClass - key) % 12 + 12) % 12;
    for (int i = 0; i < scaleSize (scale); ++i) if (scaleIntervals (scale)[i] == rel) return true;
    return false;
}

class Glass
{
public:
    static constexpr int kVoices = 6;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const int n = Ring::pow2AtLeast (sampleRate / 40.0);
        for (auto& ch : voice) for (auto& v : ch) { v.ring.init (n); }
        reset();
        setTuning (0, 0, 0.4f);
        for (auto& ch : voice) for (auto& v : ch) v.cur = v.target;
        setDecay (0.5f);
    }
    void reset()
    {
        for (auto& ch : voice) for (auto& v : ch) { v.ring.clear(); v.lp = 0.0f; }
    }

    void setTuning (int key, int scale, float spread01)
    {
        const int n = scaleSize (scale);
        const int idx[kVoices] = { 0, 2, 4, n, n + 2, n + 4 };
        const float cents = spread01 * 14.0f;
        for (int i = 0; i < kVoices; ++i)
        {
            const int midi = 60 + key + scaleIntervals (scale)[idx[i] % n] + 12 * (idx[i] / n);
            const float f = 440.0f * std::pow (2.0f, ((float) midi - 69.0f) / 12.0f);
            voice[0][i].target = sr / (f * std::pow (2.0f, cents / 1200.0f));
            voice[1][i].target = sr / (f * std::pow (2.0f, -cents / 1200.0f));
        }
    }

    void setDecay (float d01)
    {
        const float t60 = 0.25f + 6.0f * std::pow (d01, 1.5f);
        for (auto& ch : voice)
            for (auto& v : ch)
            {
                const float d = juce::jmax (2.0f, v.target);
                v.g = juce::jmin (0.9985f, std::pow (10.0f, -3.0f * d / (sr * t60)));
            }
    }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        float s[2] = { 0.0f, 0.0f };
        const float x[2] = { inL, inR };
        for (int c = 0; c < 2; ++c)
            for (auto& v : voice[c])
            {
                v.cur += (v.target - v.cur) * 0.0008f;
                const float fb = v.ring.read (juce::jmax (1.0f, v.cur - 1.0f));
                v.lp += 0.55f * (fb - v.lp);
                const float y = x[c] * (1.0f - v.g) * 0.9f + v.g * v.lp;
                v.ring.push (y);
                s[c] += y;
            }
        outL = s[0] * 0.5f;
        outR = s[1] * 0.5f;
        level += (std::abs (outL) + std::abs (outR) - level) * 0.0004f;
    }

    float level = 0.0f;

private:
    struct Voice { Ring ring; float cur = 200.0f, target = 200.0f, g = 0.99f, lp = 0.0f; };
    Voice voice[2][kVoices];
    float sr = 44100.0f;
};
} // namespace pinky
