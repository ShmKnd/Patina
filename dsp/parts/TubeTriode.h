#pragma once
#include <cmath>
#include <algorithm>
#include <random>

// Vacuum tube (triode) primitive — 12AX7 / 6386 etc.
// Fundamental component for preamp, variable-mu compressor
//
// 12AX7 properties:
//   plate characteristics: asymmetric tanh (even harmonics)
//   grid conduction: grid current when Vg > 0
//   Miller capacitance: C_gp ≈ 1.7pF × (1 + Av) → treble rolloff
//   microphonics: mechanical vibration noise
//   emission degradation: cathode lifetime model
//
// Usage example:
//   TubeTriode tube(TubeTriode::T12AX7());
//   tube.prepare(44100.0);
//   double out = tube.transferFunction(input, ageFactor);
class TubeTriode
{
public:
    struct Spec
    {
        double plateR         = 62500.0;  // Plate resistance rp (Ω)
        double plateCap       = 3.2e-12;  // plate capacitance (F)
        double millerCap      = 1.7e-12;  // Miller capacitance C_gp (F)
        double millerAv       = 60.0;     // amplification factor (Miller effect multiplier)
        double gridR          = 68e3;     // Grid resistance (Ω)
        double micFreq        = 180.0;    // microphonics center frequency (Hz)
        double micLevel       = 2e-5;     // microphonics level
        double shotNoise      = 1e-5;     // shot noise level
        double emissionLossMax = 0.4;     // maximum emission degradation rate
    };

    // 6386: variable-mu tube
    struct VariableMuSpec
    {
        double muCoeff       = 3.0;    // gm exponential coefficient
        double minGain       = 0.001;  // maximum reduction (-60dB)
        double gainSlewRate  = 0.05;   // Gain slew rate
    };

    // 12AX7 (ECC83) — most widely used tube, from guitar amp first stage to Hi-Fi preamp
    // High-mu tube. Gain ~100, high rp, even harmonics during overdrive are
    // 'tube warmth.' Beloved across blues, rock, jazz
    static constexpr Spec T12AX7()  { return { 62500.0, 3.2e-12, 1.7e-12, 60.0, 68e3,  180.0, 2e-5, 1e-5, 0.4 }; }
    // 12AT7 (ECC81) — medium-mu tube. Reverb driver, phase inverter, etc.
    // Excels where drive current needed. Essential to spring reverb's lush tone
    static constexpr Spec T12AT7()  { return { 11000.0, 2.2e-12, 1.4e-12, 30.0, 100e3, 200.0, 1e-5, 8e-6, 0.35 }; }
    // 12BH7 — low-mu, high-current tube. Power amp driver stage or
    // headphone amp output stage. Clean headroom at large amplitudes
    static constexpr Spec T12BH7()  { return { 5300.0,  4.5e-12, 2.0e-12, 15.0, 47e3,  150.0, 3e-5, 1.2e-5, 0.3 }; }

    // 6386 — rare variable-mu tube used in 1950-60s broadcast limiters.
    // Dual triode with 'remote cutoff' where gm varies with control voltage;
    // created broadcast standard with program-dependent smooth compression
    static constexpr VariableMuSpec Tube6386() { return { 3.0, 0.001, 0.05 }; }

    TubeTriode() noexcept = default;
    explicit TubeTriode(const Spec& s) noexcept : spec(s) {}

    void prepare(double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        double dt = 1.0 / sampleRate;
        // Miller capacitance LPF: C_effective = C_gp × (1 + Av)
        double millerCeff = spec.millerCap * (1.0 + spec.millerAv);
        double millerRC = spec.gridR * millerCeff;
        millerAlpha = dt / (millerRC + dt);
        millerAlpha = std::clamp(millerAlpha, 0.001, 1.0);
        // plate impedance LPF
        double plateRC = spec.plateR * spec.plateCap;
        plateAlpha = dt / (plateRC + dt);
        plateAlpha = std::clamp(plateAlpha, 0.001, 1.0);
    }

    void reset() noexcept
    {
        millerState = 0.0;
        plateState = 0.0;
        gridCapState = 0.0;
        micPhase = 0.0;
    }

    // 12AX7 plate characteristic (asymmetric tanh → even harmonics)
    static inline double transferFunction(double x, double ageFactor = 1.0) noexcept
    {
        if (x >= 0.0)
        {
            double satPoint = 0.8 * ageFactor;
            return std::tanh(x * satPoint) * 1.2;
        }
        else
        {
            double absx = -x;
            if (absx < 1.5)
                return -std::tanh(absx * 1.5) * 0.8;
            else
            {
                // Clamp linear tail to prevent unbounded negative output
                double tail = -0.8 - (absx - 1.5) * 0.05;
                return std::max(tail, -1.0);
            }
        }
    }

    // output stage soft saturation (even harmonics)
    static inline double softSaturate(double x) noexcept
    {
        double absX = std::abs(x);
        if (absX < 0.5) return x;
        double sign = (x > 0.0) ? 1.0 : -1.0;
        return sign * (0.5 + (absX - 0.5) / (1.0 + (absX - 0.5) * 2.0));
    }

    // push-pull balance (even harmonic cancellation → variable-mu transparency)
    static inline double pushPullBalance(double x) noexcept
    {
        if (std::abs(x) < 0.3) return x;
        return std::tanh(x * 1.2) / 1.2;
    }

    // output stage saturation (12BH7 plate limiter — odd harmonics)
    static inline double outputSaturate(double x) noexcept
    {
        if (std::abs(x) < 1.0) return x;
        return std::tanh(x);
    }

    // Variable-mu gain: gm ∝ exp(-k × Vg2)
    static inline double variableMuGain(double controlVoltage, const VariableMuSpec& vmu) noexcept
    {
        double gain = std::exp(-vmu.muCoeff * controlVoltage);
        return std::clamp(gain, vmu.minGain, 1.0);
    }

    // Miller capacitance filter
    inline double processMiller(double x) noexcept
    {
        millerState += millerAlpha * (x - millerState);
        return millerState;
    }

    // plate impedance filter
    inline double processPlateImpedance(double x) noexcept
    {
        plateState += plateAlpha * (x - plateState);
        return x * 0.95 + plateState * 0.05;
    }

    // grid conduction
    inline double processGridConduction(double x) noexcept
    {
        const double gridAlpha = 0.001;
        if (x > 0.0)
        {
            gridCapState += gridAlpha * (x - gridCapState);
            return x - gridCapState * 0.3;
        }
        else
        {
            gridCapState *= (1.0 - gridAlpha * 0.1);
            return x;
        }
    }

    const Spec& getSpec() const noexcept { return spec; }

    double millerAlpha  = 0.5;
    double plateAlpha   = 0.5;
    double millerState  = 0.0;
    double plateState   = 0.0;
    double gridCapState = 0.0;
    double micPhase     = 0.0;

private:
    Spec spec = T12AX7();
    double sampleRate = 44100.0;
};
