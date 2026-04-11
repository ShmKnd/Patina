#pragma once
#include <cmath>
#include <algorithm>
#include "../constants/DiodeConstants.h"

// Diode primitive — base-level component model
// Parametrizes Si / Schottky / Ge diode material properties
// Provides temperature-dependent Vf, junction capacitance, reverse recovery, nonlinear I-V curve
//
// Usage example:
//   DiodePrimitive d(DiodePrimitive::Si1N4148());
//   double clipped = d.clip(inputVoltage);
//   double saturated = d.saturate(inputVoltage);
class DiodePrimitive
{
public:
    struct Spec
    {
        double Vf_25C      = 0.65;    // forward voltage @ 25°C (V)
        double tempCoeff   = -0.002;  // temperature coefficient (V/°C)
        double seriesR     = 1.0;     // Bulk series resistance (Ω)
        double slopeScale  = 40.0;    // exponential saturation curve steepness
        double asymmetry   = 1.0;     // positive/negative asymmetry (1.0=symmetric)
        double junctionCap = 4e-12;   // junction capacitance (F)
        double recoveryTime = 4e-9;   // reverse recovery time (s)
    };

    // factory: datasheet-based presets
    // General-purpose fast-switching diode — standard symmetric clipper for overdrive pedals
    static constexpr Spec Si1N4148()    { return { 0.65, -0.002,  1.0, 40.0, 0.97, 4e-12,   4e-9  }; }
    // Schottky barrier — low Vf for soft clipping, widely used in boutique pedals
    static constexpr Spec Schottky1N5818() { return { 0.30, -0.001, 10.0, 30.0, 0.99, 20e-12,  0.5e-9 }; }
    // Germanium diode — warm asymmetric clipping from 1960s fuzz circuits
    static constexpr Spec GeOA91()      { return { 0.25, -0.002, 50.0, 25.0, 0.93, 2e-12,  10e-9  }; }
    // Low Vf silicon — diode ladder as exemplified by classic Japanese acid synthesizer
    // Clipping element creating distinctive "squelchy" resonance in filter circuits
    static constexpr Spec LowVfSilicon()  { return { 0.55, -0.002,  2.0, 35.0, 0.95, 5e-12,   3e-9  }; }
    // OTA filter input protection diode — adopted in late 1970s Japanese analog synthesizers
    // Input-stage clipper of OTA Sallen-Key filter. Determines distortion at large input
    static constexpr Spec OtaInputDiode()   { return { 0.60, -0.002,  1.5, 38.0, 1.00, 3e-12,   4e-9  }; }

    DiodePrimitive() noexcept = default;
    explicit DiodePrimitive(const Spec& s) noexcept : spec(s) {}

    // effective Vf accounting for temperature
    double effectiveVf(double temperature = 25.0) const noexcept
    {
        double vf = spec.Vf_25C + spec.tempCoeff * (temperature - 25.0);
        return std::max(0.05, vf);
    }

    // diode saturation (tanh-based — soft clipping)
    inline double saturate(double x, double temperature = 25.0) const noexcept
    {
        double vf = effectiveVf(temperature);
        double scale = 1.0 / vf;
        return std::tanh(x * scale * 0.5) * vf * 2.0;
    }

    // diode clipping (asymmetric hard/soft)
    inline double clip(double x, double temperature = 25.0) const noexcept
    {
        double vf = effectiveVf(temperature);
        double vthPos = vf;
        double vthNeg = vf * spec.asymmetry;
        double slope = spec.slopeScale;

        if (x > 0.0)
        {
            if (x > vthPos)
            {
                double excess = x - vthPos;
                double clipV = vthPos + (1.0 - std::exp(-slope * excess)) / slope;
                return clipV + excess * spec.seriesR * 0.001;
            }
        }
        else
        {
            double ax = -x;
            if (ax > vthNeg)
            {
                double excess = ax - vthNeg;
                double clipV = vthNeg + (1.0 - std::exp(-slope * excess)) / slope;
                return -(clipV + excess * spec.seriesR * 0.001);
            }
        }
        return x;
    }

    // soft clip for feedback path
    inline double feedbackClip(double x, double temperature = 25.0) const noexcept
    {
        double vf = effectiveVf(temperature);
        if (x >  vf) return  vf + (x - vf) * 0.1;
        if (x < -vf) return -vf + (x + vf) * 0.1;
        return x;
    }

    // junction capacitance (voltage-dependent): C(V) = C0 / sqrt(1 + V/Vbi)
    double junctionCapacitance(double voltage) const noexcept
    {
        const double Vbi = 0.7;
        double absV = std::min(std::abs(voltage), 10.0);
        return spec.junctionCap / std::sqrt(1.0 + absV / Vbi);
    }

    const Spec& getSpec() const noexcept { return spec; }

private:
    Spec spec = Si1N4148();
};
