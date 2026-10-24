// Pinky Lite 2 - tempo-synced stutter / reverse / tape-stop / gate
// Author: seoulbby
#pragma once
#include "Ring.h"
#include <juce_core/juce_core.h>
#include <atomic>

namespace pinky
{
enum GlitchType { Stutter = 0, Reverse, TapeStop, Gate, RandomType, NumGlitchTypes };
inline constexpr const char* glitchTypeNames[NumGlitchTypes] = { "Stutter", "Reverse", "Tape stop", "Gate", "Random" };
inline constexpr const char* glitchDivNames[] = { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
inline constexpr int numGlitchDivs = 6;
inline double glitchDivBeats (int i) noexcept
{
    static constexpr double v[numGlitchDivs] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    return v[std::clamp (i, 0, numGlitchDivs - 1)];
}

class Glitch
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int n = Ring::pow2AtLeast (6.0 * sampleRate);
        ring[0].init (n); ring[1].init (n);
        reset();
    }
    void reset()
    {
        ring[0].clear(); ring[1].clear();
        stepPos = 0.0; stepIdx = -1; wc = 0; active = false; fade = 0.0f; gateSm = 1.0f; type = 0;
        history = 0; posOut = 0.0f; typeOut = -1;
    }

    // call once per chunk. ppq only used if `locked`.
    void setTiming (double bpm, double stepBeats, bool locked, double ppq, int typeSel, float chance_, bool repeatable_)
    {
        sps = juce::jmax (256.0, stepBeats * 60.0 / bpm * sr);
        typeSelect = typeSel; chance = chance_; repeatable = repeatable_;
        if (locked)
        {
            const double x = ppq / stepBeats;
            const auto idx = (juce::int64) std::floor (x);
            if (idx != stepIdx) { stepIdx = idx; trigger(); }
            stepPos = x - (double) idx;
        }
    }

    void process (float& l, float& r, float amount, float smoothMs) noexcept
    {
        ring[0].push (l); ring[1].push (r);
        ++wc;

        stepPos += 1.0 / sps;
        if (stepPos >= 1.0) { stepPos -= 1.0; ++stepIdx; trigger(); }

        const float coef = 1.0f - std::exp (-1.0f / (juce::jmax (0.5f, smoothMs) * 0.001f * (float) sr));
        fade += ((active ? 1.0f : 0.0f) - fade) * coef;
        if (fade < 0.0005f && ! active) { posOut = (float) stepPos; return; }

        float gl = l, gr = r;
        switch (type)
        {
            case Stutter:
            {
                const juce::int64 ab = startAbs + (pos % slice);
                const int d = (int) ((wc - 1) - ab);
                gl = ring[0].readInt (d); gr = ring[1].readInt (d);
                const int p = (int) (pos % slice);
                const float w = juce::jmin (1.0f, (float) p / 48.0f, (float) (slice - p) / 48.0f);
                gl *= w; gr *= w;
                ++pos;
                break;
            }
            case Reverse:
                gl = ring[0].read (revD); gr = ring[1].read (revD);
                revD = juce::jmin (revD + 1.0f, (float) sps);
                break;
            case TapeStop:
                gl = ring[0].read (stopD); gr = ring[1].read (stopD);
                stopD += 1.0f - speed;
                speed = juce::jmax (0.0f, speed - (float) (1.0 / (sps * 0.85)));
                break;
            default: // Gate
            {
                const float g = std::fmod (stepPos * 4.0, 1.0) < 0.5 ? 1.0f : 0.0f;
                gateSm += (g - gateSm) * 0.02f;
                gl = l * gateSm; gr = r * gateSm;
                break;
            }
        }
        const float a = amount * fade;
        l += (gl - l) * a;
        r += (gr - r) * a;
        posOut = (float) stepPos;
    }

    // for the UI (4 bits per step, newest in the lowest nibble: 0 = none, type+1 otherwise)
    std::atomic<std::uint32_t> history { 0 };
    std::atomic<float> posOut { 0.0f };
    std::atomic<int> typeOut { -1 };

private:
    void trigger()
    {
        juce::int64 seed = repeatable ? (juce::int64) (((stepIdx % 64) + 64) % 64) * 2654435761LL + 977 : (juce::int64) rng.nextInt();
        juce::Random r (seed);
        r.nextInt(); r.nextInt(); r.nextInt();

        active = r.nextFloat() < chance;
        if (! active) { history = (history.load() << 4) & 0xffffffffu; typeOut = -1; return; }

        type = typeSelect >= RandomType ? r.nextInt (4) : typeSelect;
        switch (type)
        {
            case Stutter:
            {
                static constexpr int divs[3] = { 2, 4, 8 };
                slice = juce::jmax (64, (int) (sps / (double) divs[r.nextInt (3)]));
                startAbs = wc - slice; pos = 0;
                break;
            }
            case Reverse:  revD = 0.0f; break;
            case TapeStop: speed = 1.0f; stopD = 0.0f; break;
            default: break;
        }
        history = ((history.load() << 4) | (std::uint32_t) (type + 1)) & 0xffffffffu;
        typeOut = type;
    }

    Ring ring[2];
    double sr = 44100.0, sps = 22050.0, stepPos = 0.0;
    juce::int64 stepIdx = -1, wc = 0, startAbs = 0, pos = 0;
    int type = 0, typeSelect = 0, slice = 1024;
    float chance = 0.5f, fade = 0.0f, revD = 0.0f, stopD = 0.0f, speed = 1.0f, gateSm = 1.0f;
    bool active = false, repeatable = true;
    juce::Random rng { 99 };
};
} // namespace pinky
