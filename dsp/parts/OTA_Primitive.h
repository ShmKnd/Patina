#pragma once
#include <cmath>
#include <algorithm>
#include <random>
#include "../constants/OpAmpConstants.h"

// OTA (Operational Transconductance Amplifier) primitive
// Models gm characteristics, nonlinearity, temperature dependence of LM13700/CA3080-type OTAs
//
// OTA became a key component in analog synthesizers from the late 1970s.
// Since gm varies with control current (Iabc), ideal for gain cells of VCF, VCA, compressor;
// Ideal as a gain cell, used in Japanese synthesizer filter circuits and chorus/
// LFO mixers of Japanese synthesizers and effects of the era.
//
// OTA Physical properties:
//   gm = Iabc / (2 × Vt)   (Vt = kT/q ≈ 26mV @ 25°C)
//   large signal: gm curve saturates to tanh
//   input differential pair mismatch: ±1.5% (manufacturing tolerance)
//   temperature: gm ∝ 1/Vt → gm decreases at high temperature
//
// Usage example:
//   OTA_Primitive ota;
//   ota.prepare(44100.0);
//   double out = ota.process(input, gCoeff);
class OTA_Primitive
{
public:
    struct Spec
    {
        double thermalNoise  = 3e-6;    // input-referred noise
        double tempCoeffGm   = -0.003;  // gm temperature coefficient (/°C)
        double saturationV   = 2.5;     // saturation voltage (V)
        double mismatchSigma = 0.015;   // differential pair mismatch σ
    };

    // presets
    // LM13700 — dual OTA introduced in 1979. The 'analog standard' still in production.
    // Widely used as gain cells in VCF, VCA, and compressor circuits; modular synthesizer
    // known for its universality — often called 'the go-to 13700'
    static constexpr Spec LM13700() { return { 3e-6, -0.003, 2.5, 0.015 }; }
    // CA3080 — single OTA predecessor to LM13700. Early analog synthesizers and
    // effects circuits; now discontinued. Softer saturation than LM13700;
    // shaped the tone of vintage phasers and wah pedals
    static constexpr Spec CA3080()  { return { 4e-6, -0.003, 2.0, 0.020 }; }

    OTA_Primitive() noexcept = default;
    explicit OTA_Primitive(const Spec& s, unsigned int seed = 211) noexcept
        : spec(s)
    {
        std::minstd_rand initRng(seed);
        std::normal_distribution<double> tol(1.0, s.mismatchSigma);
        mismatch = std::clamp(tol(initRng), 0.93, 1.07);
    }

    // gm saturation function (tanh): LM13700 large-signal characteristic
    inline double saturate(double x) const noexcept
    {
        double sat = spec.saturationV;
        if (x >  sat) return  sat;
        if (x < -sat) return -sat;
        double invSat = 1.0 / sat;
        return std::tanh(x * invSat * 0.4) * sat;
    }

    // temperature-dependent gm scale
    double gmScale(double temperature = 25.0) const noexcept
    {
        double delta = temperature - 25.0;
        double scale = 1.0 + spec.tempCoeffGm * delta;
        return std::clamp(scale, 0.85, 1.15);
    }

    // integrator step: y = s + g * mismatch * (sat(in) - sat(s))
    inline double integrate(double input, double state, double gCoeff) const noexcept
    {
        return state + gCoeff * mismatch * (saturate(input) - saturate(state));
    }

    double getMismatch() const noexcept { return mismatch; }
    const Spec& getSpec() const noexcept { return spec; }

private:
    Spec spec = LM13700();
    double mismatch = 1.0;
};
