// Pinky Lite 2 - waveshapers and LFO curves
// Author: seoulbby
#pragma once
#include <cmath>
#include <algorithm>

namespace pinky
{
enum Mode { Soft = 0, Tube, Tape, Fuzz, Fold, Warp, Clip, Rect, NumModes };

inline constexpr const char* modeNames[NumModes] = { "Soft", "Tube", "Tape", "Fuzz", "Fold", "Warp", "Clip", "Rect" };

// x is the already-driven sample, s = "shape" 0..1, bias -1..1
inline float shapeSample (int mode, float x, float s, float bias) noexcept
{
    x += bias * 0.6f;

    switch (mode)
    {
        case Soft: // tanh <-> algebraic knee
        {
            const float a = std::tanh (x);
            const float b = x / (1.0f + std::abs (x));
            return a + (b - a) * s;
        }
        case Tube: // asymmetric exponential saturation, shape = amount of asymmetry
        {
            const float k = 1.0f + s * 2.5f;
            return x >= 0.0f ? 1.0f - std::exp (-x) : -(1.0f - std::exp (x * k)) / k;
        }
        case Tape: // generalised soft clip, shape = knee hardness
        {
            const float p = 1.2f + s * 3.5f;
            return x / std::pow (1.0f + std::pow (std::abs (x), p), 1.0f / p);
        }
        case Fuzz: // fast exponential, gated-ish at low levels
        {
            const float k = 1.0f + s * 5.0f;
            const float y = 1.0f - std::exp (-std::abs (x) * k);
            return x < 0.0f ? -y : y;
        }
        case Fold: // sine wavefolder
            return std::sin (x * (1.0f + s * 2.0f)) / (1.0f + s * 0.5f);
        case Warp: // phase-warp: lots of odd/even harmonics
        {
            const float c = std::tanh (x);
            const float m = 1.0f + 5.0f * s;
            return std::sin (m * std::asin (std::clamp (c, -1.0f, 1.0f)));
        }
        case Clip: // hard clip <-> tanh
        {
            const float h = std::clamp (x, -1.0f, 1.0f);
            return h + (std::tanh (x) - h) * s;
        }
        case Rect: // rectifier (octave-up flavour)
        {
            const float t = s * 0.9f;
            return std::tanh ((1.0f - t) * x + t * std::abs (x));
        }
        default: break;
    }
    return x;
}

// ---------------------------------------------------------------- LFO
enum LfoShape { Sine = 0, Triangle, SawDown, Square, Pump, Random, NumLfoShapes };
inline constexpr const char* lfoShapeNames[NumLfoShapes] = { "Sine", "Triangle", "Saw", "Square", "Pump", "Random" };

// bipolar -1..1, phase 0..1
inline float lfoEval (int shape, float p) noexcept
{
    constexpr float twoPi = 6.2831853f;
    switch (shape)
    {
        case Sine:     return std::sin (twoPi * p);
        case Triangle: return 4.0f * std::abs (p - 0.5f) - 1.0f;
        case SawDown:  return 1.0f - 2.0f * p;
        case Square:   return p < 0.5f ? 1.0f : -1.0f;
        case Pump:     return 2.0f * (1.0f - std::exp (-5.0f * p)) / (1.0f - std::exp (-5.0f)) - 1.0f;
        default:       return 0.0f;
    }
}

// beats (quarter notes) per LFO cycle, index 0 = Free
inline constexpr const char* syncNames[] = { "Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/4D", "1/8D" };
inline constexpr int numSync = 12;
inline double syncBeats (int idx) noexcept
{
    static constexpr double v[numSync] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                           2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75 };
    return v[std::clamp (idx, 0, numSync - 1)];
}
} // namespace pinky
