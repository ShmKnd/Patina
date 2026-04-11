#pragma once
#include <cmath>
#include <algorithm>
#include <random>

// VCA (Voltage Controlled Amplifier) chip primitive
// Models gain characteristics of THAT 2180 / classic dynamics processor 2150 series Blackmer cells
//
// VCA chip structure:
//   Log/antilog gain cell using matched BJT pairs.
//   input current log compression → control current addition → antilog expansion for output.
//   This enables exponential (dB-linear) gain control。
//
// VCA Physical properties:
//   THD: primarily 3rd harmonic (BJT saturation) + 2nd (pair mismatch)
//   dynamic range: >120dB (THAT 2180)
//   Temperature: gain tracking ≈ ±0.003 dB/°C
//   pair mismatch: gm variance causes 2nd harmonic distortion
//
// Usage example:
//   VcaPrimitive vca(VcaPrimitive::THAT2180());
//   double out = vca.applyGain(input, 0.5); // -6dB gain
class VcaPrimitive
{
public:
    struct Spec
    {
        double pairMismatchSigma = 0.003;  // matched BJT pair mismatch σ
        double thd3rdCoeff       = 0.0008; // 3rd harmonic distortion coefficient (from BJT saturation)
        double thd2ndCoeff       = 0.0003; // 2nd harmonic distortion coefficient (from pair mismatch)
        double thermalNoise      = 1e-6;   // input-referred noise
        double tempCoeffGain     = -0.003; // Gain temperature coefficient (dB/°C)
        double saturationLevel   = 10.0;   // output rail saturation voltage (normalized)
    };

    // THAT 2180 — perfected Blackmer cell. Classic large-format console bus comp,
    // British console 33609 revisions; widely used in pro audio. Ultra-low distortion, wide
    // dynamic range; industry-standard VCA chip
    static constexpr Spec THAT2180() { return { 0.003, 0.0008, 0.0003, 1e-6, -0.003, 10.0 }; }

    // dbx 2150 — original VCA cell. Integrated in classic dynamics processor 160/165/900 series
    // Slightly larger pair mismatch and more 3rd harmonic than THAT 2180.
    // Possesses subtle warmth known as 'classic dynamics processor tone'
    static constexpr Spec DBX2150()  { return { 0.005, 0.0012, 0.0005, 1.5e-6, -0.003, 8.0 }; }

    VcaPrimitive() noexcept = default;
    explicit VcaPrimitive(const Spec& s, unsigned int seed = 500) noexcept
        : spec(s)
    {
        std::minstd_rand initRng(seed);
        std::normal_distribution<double> tol(1.0, s.pairMismatchSigma);
        pairMismatch = std::clamp(tol(initRng), 0.995, 1.005);
    }

    // VCA Gain application — includes nonlinear characteristics of log/antilog BJT pair
    // input: Input signal, gainLinear: linear gain (0.001~1.0)
    inline double applyGain(double input, double gainLinear) const noexcept
    {
        // Matched pair gain tracking (during the log → add → antilog process
        // Pair mismatch appears as slight gain error)
        double v = input * gainLinear * pairMismatch;

        // 2nd harmonic — from BJT pair mismatch
        // imperfect log-antilog pair accuracy produces even-order distortion
        double mismatchDelta = pairMismatch - 1.0;
        v += mismatchDelta * spec.thd2ndCoeff * v * std::abs(v);

        // 3rd harmonic — from BJT saturation, proportional to gain reduction
        // During large gain reduction, the logarithmic cell operating point enters the nonlinear region
        double gainReduction = std::max(0.0, 1.0 - gainLinear);
        double thd3 = spec.thd3rdCoeff * gainReduction;
        v += thd3 * v * v * v;

        // output rail saturation (Blackmer cell soft clip)
        double sat = spec.saturationLevel;
        if (std::abs(v) > sat * 0.8)
        {
            v = std::tanh(v / sat) * sat;
        }

        return v;
    }

    // Temperature-dependent gain tracking error
    double tempGainScale(double temperature = 25.0) const noexcept
    {
        double delta = temperature - 25.0;
        double errorDb = delta * spec.tempCoeffGain;
        return std::pow(10.0, errorDb / 20.0);
    }

    double getPairMismatch() const noexcept { return pairMismatch; }
    const Spec& getSpec() const noexcept { return spec; }

private:
    Spec spec = THAT2180();
    double pairMismatch = 1.0;
};
