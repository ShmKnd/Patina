#pragma once
#include <cmath>
#include <algorithm>
#include <random>

// BJT (bipolar junction transistor) primitive
// Foundation of differential pairs / cascades for transistor ladder filters in traditional subtractive synthesizers
//
// Physical properties:
//   gm = Ic / Vt   (Vt = kT/q ≈ 26mV @ 25°C)
//   temperature dependence: Vbe ≈ -2mV/°C → cutoff frequency drifts
//   large signal: tanh saturation (differential pair)
//   pair mismatch: ±2% (manufacturing tolerance)
//   thermal noise: Johnson-Nyquist
class BJT_Primitive
{
public:
    struct Spec
    {
        double mismatchSigma    = 0.02;   // gm variance σ
        double tempCoeffVbe     = -0.002; // Vbe temperature coefficient (V/°C)
        double resonanceDamping = 0.5;    // large-signal resonance damping
        double interStageCapAlpha = 0.005;// inter-stage capacitive coupling coefficient
        double thermalNoise     = 3e-6;   // thermal noise level
    };

    // generic — NPN transistor pair with standard manufacturing tolerance. Ladder filters or
    // general discrete circuit behavior reproduction
    static constexpr Spec Generic()  { return { 0.02, -0.002, 0.5, 0.005, 3e-6 }; }
    // matched pair — pair selected at manufacture with matched gm and beta.
    // In transistor ladder filters, pair accuracy determines resonance peak and
    // 4-pole tracking; essential selection process in vintage instruments
    static constexpr Spec Matched()  { return { 0.005, -0.002, 0.5, 0.003, 2e-6 }; }

    BJT_Primitive() noexcept = default;
    explicit BJT_Primitive(const Spec& s, unsigned int seed = 123) noexcept
        : spec(s)
    {
        std::minstd_rand initRng(seed);
        std::normal_distribution<double> tol(1.0, s.mismatchSigma);
        mismatch = std::clamp(tol(initRng), 0.92, 1.08);
    }

    // differential pair saturation (tanh)
    static inline double saturate(double x) noexcept
    {
        return std::tanh(x);
    }

    // temperature-dependent cutoff scale
    double tempScale(double temperature = 25.0) const noexcept
    {
        double delta = temperature - 25.0;
        double scale = 1.0 + delta * (-0.003); // fc ≈ -0.3%/°C
        return std::clamp(scale, 0.85, 1.15);
    }

    // 1-stage LPF step: y = s + g * mismatch * (tanh(in) - tanh(s))
    inline double integrate(double input, double state, double gCoeff) const noexcept
    {
        return state + gCoeff * mismatch * (saturate(input) - saturate(state));
    }

    // inter-stage capacitive coupling
    inline double interStageCoupling(double y, double& capState) const noexcept
    {
        capState += spec.interStageCapAlpha * (y - capState);
        return y * 0.998 + capState * 0.002;
    }

    double getMismatch() const noexcept { return mismatch; }
    const Spec& getSpec() const noexcept { return spec; }

private:
    Spec spec = Generic();
    double mismatch = 1.0;
};
