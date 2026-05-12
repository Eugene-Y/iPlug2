#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <tuple>


namespace hvoya {


// Map a normalized parameter v ∈ [0, 1] to (optionIdx, weight) for blending
// between consecutive options of a SegmentKnob-style N-option control.
//
// Geometry matches SegmentKnob's tick layout exactly:
//   halfStable = w / (2n)                  (same as the knob's tick half-width
//                                           on a unit arc)
//   option centers are inset:
//     pos_0     = halfStable
//     pos_{n-1} = 1 - halfStable
//     step      = (1 - 2*halfStable) / (n - 1)
//   stable region of option i: [pos_i - halfStable, pos_i + halfStable]
//   blend region fills the gap between consecutive stable regions
//
// Output (i, c):
//   i ∈ [0, n - 1]
//   c ∈ [0, 1] — weight of option i; weight of option i + 1 is (1 - c)
//   Inside any stable region: c = 1.
//   Inside blend region k -> k + 1: c falls linearly 1 -> 0.
//
// Consumer pattern:
//   auto [i, c] = mapNormToOptionBlend(v, n, w);
//   result = c * option[i] + (1 - c) * option[std::min(i + 1, n - 1)];
//
// n must be > 0. For n == 1 we always return (0, 1).
inline std::tuple<int, double>
mapNormToOptionBlend (double v, int n, double stableRegionNormalizedWidth) {
    assert (n > 0);
    if (n == 1)
        return {0, 1.0};

    v = std::clamp (v, 0.0, 1.0);
    const double w = std::clamp (stableRegionNormalizedWidth, 0.0, 1.0);

    const double halfStable = w * 0.5 / double (n);

    if (v <= halfStable)
        return {0, 1.0};
    if (v >= 1.0 - halfStable)
        return {n - 1, 1.0};

    const double step    = (1.0 - 2.0 * halfStable) / double (n - 1);
    const double innerV  = v - halfStable;
    const int    i       = std::min (int (std::floor (innerV / step)), n - 2);
    const double t       = innerV - double (i) * step;

    const double blendStart = halfStable;
    const double blendEnd   = step - halfStable;

    if (t <= blendStart)
        return {i, 1.0};
    if (t >= blendEnd)
        return {i + 1, 1.0};

    const double c = 1.0 - (t - blendStart) / (blendEnd - blendStart);
    return {i, c};
}


} // namespace hvoya
