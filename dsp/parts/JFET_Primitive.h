#pragma once
#include <cmath>
#include <algorithm>
#include <random>

// JFET primitive — junction FET model such as 2N5457
// Models characteristics of voltage-controlled variable-resistance JFETs
// Fundamental component for VCA, phaser, compressor
//
// JFET Physical properties:
//   Ids = Idss × (1 - Vgs/Vp)²  (pinch-off region)
//   Vp: pinch-off voltage (temperature-dependent: -2mV/°C)
//   gm = 2 × Idss / |Vp| × (1 - Vgs/Vp)
//   large signal: drain current saturation
//   1/f Noise dominant
//
// Usage example:
//   JFET_Primitive jfet(JFET_Primitive::N2N5457());
//   double out = jfet.softClip(input);
//   double gr = jfet.vcaGain(controlVoltage);
class JFET_Primitive
{
public:
    struct Spec
    {
        double Vp            = -3.0;    // pinch-off voltage (V)
        double vpTempCoeff   = -0.002;  // Vp temperature coefficient (V/°C)
        double gmMismatch    = 0.08;    // gm variance σ
        double satVoltage    = 3.0;     // drain saturation voltage (normalized)
        double noiseLevel    = 2e-6;    // 1/f noise level
    };

    // 2N5457 — known as VCA element in legendary FET compressor. Low pinch-off voltage for
    // easy control; standard for 'fast and clean' compression in studios and mastering
    static constexpr Spec N2N5457() { return { -3.0, -0.002, 0.08, 3.0, 2e-6 }; }
    // 2N3819 — general-purpose N-channel JFET. Used in phaser, chorus, tremolo
    // LFO control circuits and buffer stages. Deep pinch-off, strong at large signals
    static constexpr Spec N2N3819() { return { -4.0, -0.002, 0.10, 3.5, 3e-6 }; }

    JFET_Primitive() noexcept = default;
    explicit JFET_Primitive(const Spec& s, unsigned int seed = 67) noexcept
        : spec(s)
    {
        std::minstd_rand initRng(seed);
        std::normal_distribution<double> tol(1.0, s.gmMismatch);
        gmScale = std::clamp(tol(initRng), 0.88, 1.12);
    }

    // drain current saturation soft clipping
    inline double softClip(double x) const noexcept
    {
        double sv = spec.satVoltage;
        if (std::abs(x) < sv) return x;
        double sign = (x >= 0.0) ? 1.0 : -1.0;
        double excess = std::abs(x) - sv;
        return sign * (sv + (1.0 - std::exp(-0.5 * excess)) * 2.0);
    }

    // VCA gain (for FET Compressor): even harmonics during compression
    inline double vcaNonlinearity(double x, double gain) const noexcept
    {
        double distortion = (1.0 - gain) * 0.1;
        if (distortion < 0.001) return x;
        double absX = std::abs(x);
        if (absX < 0.01) return x;
        return x + distortion * x * absX * 0.5;
    }

    // temperature-dependent Vp shift → frequency deviation
    double tempFreqScale(double temperature = 25.0) const noexcept
    {
        double delta = temperature - 25.0;
        double vpShift = delta * spec.vpTempCoeff;
        double scale = 1.0 + vpShift * 10.0;
        return std::clamp(scale, 0.90, 1.10);
    }

    double getGmScale() const noexcept { return gmScale; }
    const Spec& getSpec() const noexcept { return spec; }

private:
    Spec spec = N2N5457();
    double gmScale = 1.0;
};
