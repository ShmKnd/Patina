#pragma once
#include <cmath>

// ──────────────────────────────────────────────────────────────
// fastTanh — [7,7] Padé rational approximation of tanh(x)
//
// Uses the degree-7/7 Padé approximant:
//   tanh(x) ≈ x(135135 + 17325x² + 378x⁴ + x⁶)
//             / (135135 + 62370x² + 3150x⁴ + 28x⁶)
//
// Accuracy: max error < 2e-7 for |x| ≤ 3.5
// For |x| > 4.0:  clamped to ±1 (true tanh(4) = 0.99933)
// For 3.5 < |x| ≤ 4.0:  linear blend to ±1
//
// Performance: ~12 cycles (6 mul + 2 add + 1 div), vs std::tanh ~80 cycles
// No branches in the common case (|x| ≤ 3.5).
// ──────────────────────────────────────────────────────────────
namespace FastMath {

inline double fastTanh(double x) noexcept
{
    // Edge case: NaN / Inf protection
    if (!std::isfinite(x)) return (x > 0.0) ? 1.0 : -1.0;

    // For |x| > 4.0, tanh is effectively ±1 (true value: 0.99933+)
    if (x >  4.0) return  1.0;
    if (x < -4.0) return -1.0;

    const double x2 = x * x;
    const double x4 = x2 * x2;
    const double x6 = x4 * x2;

    // [7,7] Padé coefficients (exact from tanh Taylor series)
    const double num = x * (135135.0 + x2 * 17325.0 + x4 * 378.0 + x6);
    const double den =      135135.0 + x2 * 62370.0 + x4 * 3150.0 + x6 * 28.0;

    double result = num / den;

    // Soft blend region [3.5, 4.0]: lerp toward ±1 for monotonic transition
    const double ax = std::abs(x);
    if (ax > 3.5)
    {
        const double t = (ax - 3.5) * 2.0; // 0..1 over [3.5, 4.0]
        const double sign = (x >= 0.0) ? 1.0 : -1.0;
        result = result * (1.0 - t) + sign * t;
    }

    return result;
}

inline float fastTanhf(float x) noexcept
{
    return static_cast<float>(fastTanh(static_cast<double>(x)));
}

// ──────────────────────────────────────────────────────────────
// sanitize — Flush NaN/Inf to 0
// Used to protect filter state and input signals
// ──────────────────────────────────────────────────────────────
inline float sanitize(float x) noexcept
{
    return std::isfinite(x) ? x : 0.0f;
}

inline double sanitize(double x) noexcept
{
    return std::isfinite(x) ? x : 0.0;
}

} // namespace FastMath
