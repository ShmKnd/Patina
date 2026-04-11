#pragma once
#include <cmath>
#include <algorithm>
#include <random>

// Magnetic tape / head primitive
// Models tape physics of studio reel-to-reel decks
//
// Physical properties:
//   magnetic hysteresis: coercivity + remanence
//   head gap: sin(x)/x loss → treble rolloff
//   head bump: tape speed-dependent LF resonance
//   tape hiss: 1/f + white noise (Voss-McCartney)
//   demagnetization degradation: residual magnetism → DC bias + noise
//   wow & flutter: periodic + random
class TapePrimitive
{
public:
    struct Spec
    {
        double gapWidthNew   = 5.0;    // new gap width (µm)
        double baseHissLevel = 5e-5;   // tape hiss baseline level
        double demagDcBias   = 0.001;  // demagnetization DC bias
        double demagNoise    = 3e-4;   // demagnetization noise
    };

    // High-speed deck — 15ips (38cm/s) European multitrack studio
    // recording standard. Natural compression from tape saturation creates 'glue'
    // that blends mixes; supported 1960-70s rock golden era recordings
    static constexpr Spec HighSpeedDeck()  { return { 5.0, 5e-5, 0.001, 3e-4 }; }
    // Mastering deck — 30ips (76cm/s) 1/2" 2-track U.S. mastering standard
    // Wide bandwidth, low distortion, low hiss;
    // still active in analog mastering as final mix 'polish'
    static constexpr Spec MasteringDeck() { return { 4.5, 4e-5, 0.0008, 2.5e-4 }; }

    TapePrimitive() noexcept = default;
    explicit TapePrimitive(const Spec& s) noexcept : spec(s) {}

    void reset() noexcept
    {
        hystState = 0.0;
        pinkState[0] = pinkState[1] = pinkState[2] = 0.0;
    }

    // magnetic hysteresis saturation
    inline double processHysteresis(double x, double satAmount, double coercivity = 1.0) noexcept
    {
        double satDrive = 1.0 + satAmount * 3.0;
        double input = x * satDrive;
        double delta = input - hystState;
        double alpha = 0.8 - satAmount * 0.4;
        hystState += delta * (1.0 - alpha);
        return std::tanh(hystState * (0.9 + satAmount * 0.3) * coercivity);
    }

    // gap loss fc calculation
    double gapLossFc(double tapeSpeed, double headWear) const noexcept
    {
        double gapWidth = spec.gapWidthNew * (1.0 + 2.0 * headWear);
        double speedIps = 15.0 * std::clamp(tapeSpeed, 0.5, 2.0);
        double speedMps = speedIps * 0.0254;
        return std::clamp(speedMps / (2.0 * gapWidth * 1e-6), 5000.0, 100000.0);
    }

    // pink noise generation (Voss-McCartney)
    inline double generateHiss(double white, double tapeAge) noexcept
    {
        double noiseMag = spec.baseHissLevel * (1.0 + 4.0 * tapeAge);
        pinkState[0] = 0.99886 * pinkState[0] + white * 0.0555179;
        pinkState[1] = 0.99332 * pinkState[1] + white * 0.0750759;
        pinkState[2] = 0.96900 * pinkState[2] + white * 0.1538520;
        double pink = pinkState[0] + pinkState[1] + pinkState[2] + white * 0.5362;
        pink *= 0.11;
        return (pink * 0.7 + white * 0.3) * noiseMag;
    }

    const Spec& getSpec() const noexcept { return spec; }

private:
    Spec spec = HighSpeedDeck();
    double hystState = 0.0;
    double pinkState[3] = {};
};
