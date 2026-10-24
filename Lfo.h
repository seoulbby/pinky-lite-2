// Pinky Lite 2 - LFO curves and tempo divisions
// Author: seoulbby
#pragma once
#include <cmath>
#include <algorithm>

namespace pinky
{
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
