// Pinky Lite 2 - tiny circular buffer with interpolated reads
// Author: seoulbby
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace pinky
{
struct Ring
{
    void init (int sizePow2) { buf.assign ((size_t) sizePow2, 0.0f); mask = sizePow2 - 1; w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }

    // d = samples behind the most recent sample (0 = latest), linear interpolation
    inline float read (float d) const noexcept
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const int a = (w - 1 - i) & mask, b = (a - 1) & mask;
        return buf[(size_t) a] + (buf[(size_t) b] - buf[(size_t) a]) * f;
    }
    inline float readInt (int d) const noexcept { return buf[(size_t) ((w - 1 - d) & mask)]; }

    static int pow2AtLeast (double n) { int s = 16; while ((double) s < n) s <<= 1; return s; }

    std::vector<float> buf;
    int mask = 0, w = 0;
};
} // namespace pinky
