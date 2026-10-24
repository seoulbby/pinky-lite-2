// Pinky Lite 2 - shimmer reverb (4-line FDN with a pitch-shifted feedback path)
// Author: seoulbby
#pragma once
#include "Ring.h"
#include <juce_core/juce_core.h>

namespace pinky
{
class Shimmer
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const float sc = sr / 44100.0f;
        static constexpr int base[4] = { 1789, 2053, 2411, 2819 };
        for (int i = 0; i < 4; ++i)
        {
            D[i] = (float) base[i] * sc;
            line[i].init (Ring::pow2AtLeast (D[i] * 1.2 + 64.0));
        }
        ap[0].init ((int) (113 * sc) + 1); ap[1].init ((int) (337 * sc) + 1);
        pitchRing.init (Ring::pow2AtLeast (0.2 * sampleRate));
        W = 0.08f * sr;
        reset();
        setDecay (0.6f);
    }
    void reset()
    {
        for (auto& l : line) l.clear();
        pitchRing.clear();
        for (auto& a : ap) a.clear();
        for (auto& d : damp) d = 0.0f;
        fbMono = 0.0f; d1 = 0.0f; level = 0.0f;
    }

    void setDecay (float d01)
    {
        const float rt = 0.6f + 8.4f * std::pow (d01, 1.4f);
        for (int i = 0; i < 4; ++i) g[i] = std::pow (10.0f, -3.0f * D[i] / (sr * rt));
        // keep the closed loop (reverb -> pitch shift -> reverb) comfortably below unity:
        // a reverb with per-pass gain g amplifies a sustained input by ~1/sqrt(1-g^2)
        const float gAvg = 0.25f * (g[0] + g[1] + g[2] + g[3]);
        shimFb = 0.4f * std::sqrt (juce::jmax (0.0f, 1.0f - gAvg * gAvg));
    }
    void setPitch (float semitones) { ratioTarget = std::pow (2.0f, semitones / 12.0f); }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        ratio += (ratioTarget - ratio) * 0.0008f;

        // pitch shift the previous reverb output (the "shimmer" feedback)
        pitchRing.push (fbMono);
        d1 += 1.0f - ratio;
        if (d1 >= W) d1 -= W;
        if (d1 < 0.0f) d1 += W;
        float d2 = d1 + 0.5f * W; if (d2 >= W) d2 -= W;
        const float w1 = std::sin (3.14159265f * d1 / W), w2 = std::sin (3.14159265f * d2 / W);
        const float shifted = pitchRing.read (d1) * w1 * w1 + pitchRing.read (d2) * w2 * w2;
        const float fbSig = std::tanh (shifted * shimFb);

        float x = 0.5f * (inL + inR) * 0.6f + fbSig;
        x = ap[0].process (x); x = ap[1].process (x);

        float v[4];
        for (int i = 0; i < 4; ++i)
        {
            ph[i] += (0.23f + 0.17f * (float) i) / sr; if (ph[i] >= 1.0f) ph[i] -= 1.0f;
            const float raw = line[i].read (D[i] + std::sin (6.2831853f * ph[i]) * 0.0007f * sr);
            damp[i] += 0.42f * (raw - damp[i]);
            v[i] = damp[i];
        }
        const float a = v[0] + v[1], b = v[0] - v[1], c = v[2] + v[3], d = v[2] - v[3];
        const float w[4] = { 0.5f * (a + c), 0.5f * (b + d), 0.5f * (a - c), 0.5f * (b - d) };
        for (int i = 0; i < 4; ++i) line[i].push (w[i] * g[i] + x * 0.5f);

        outL = 0.5f * (v[0] + v[2]);
        outR = 0.5f * (v[1] + v[3]);
        fbMono = 0.5f * (outL + outR);
        level += (std::abs (outL) + std::abs (outR) - level) * 0.0004f;
    }

    float level = 0.0f;

private:
    struct AP
    {
        std::vector<float> b; int i = 0;
        void init (int n) { b.assign ((size_t) n, 0.0f); i = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); }
        float process (float x) noexcept
        {
            const float wd = b[(size_t) i], wv = x + 0.6f * wd;
            b[(size_t) i] = wv;
            if (++i >= (int) b.size()) i = 0;
            return -0.6f * wv + wd;
        }
    };

    Ring line[4], pitchRing;
    AP ap[2];
    float D[4] {}, g[4] { 0.9f, 0.9f, 0.9f, 0.9f }, damp[4] {}, ph[4] { 0.0f, 0.25f, 0.5f, 0.75f };
    float sr = 44100.0f, W = 3528.0f, d1 = 0.0f, ratio = 2.0f, ratioTarget = 2.0f, fbMono = 0.0f, shimFb = 0.3f;
};
} // namespace pinky
