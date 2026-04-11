#pragma once
#include <cmath>
#include <algorithm>
#include <random>

// OpAmpPrimitive — general-purpose op-amp primitive
// Models per-IC slew rate, open-loop gain, GBW, saturation characteristics, and noise
// in a single L2 component primitive.
//
// Previously scattered across InputBuffer / BbdFeedback / ModdingConfig;
// Op-amp characteristics modeled using the same Spec + presets pattern as OTA_Primitive etc.
// allowing use from any circuit module.
//
// Model targets:
//   - slew rate limiting (treble rolloff / transient rounding)
//   - Soft saturation (positive/negative asymmetric, supply voltage dependent)
//   - input bias current noise (JFET input vs bipolar input)
//   - input offset voltage temperature drift
//   - Bandwidth limiting from open-loop gain and GBW
//   - manufacturing variance (GBW ±σ)
//
// presets IC:
//   TL072CP   — JFET input dual. Low bias current, bright and open
//   JRC4558D  — bipolar input. Low slew rate→dark, warm. Classic overdrive pedal standard
//   NE5532    — Bipolar input. Console-type density. Forward midrange
//   OPA2134   — JFET input. Wide bandwidth, low distortion. Hi-Fi clean
//   LM4562    — bipolar input. Ultra-low noise, ultra-wideband. Studio transparent
//   LM741     — bipolar input. 1968 original general-purpose IC. Low slew rate,
//               high offset, narrow bandwidth; creates vintage fuzz and effects
//               'rawness'
//
// Usage example:
//   OpAmpPrimitive amp(OpAmpPrimitive::NE5532());
//   amp.prepare(44100.0);
//   double out = amp.process(input);       // Saturation + slew rate limiting
//   double sat = amp.saturate(input);      // Saturation only
//   double slew = amp.slewLimit(input);    // Slew rate limiting only
class OpAmpPrimitive
{
public:
    struct Spec
    {
        const char* name          = "TL072CP";
        double slewRate           = 13.0e6;     // slew rate [V/s]
        double openLoopGain       = 2e5;        // Open-loop voltage gain (Aol)
        double gbwHz              = 3e6;        // Gain-bandwidth product [Hz]

        // saturation characteristics — 18V supply
        double satThreshold18V    = 0.88;       // full scale ratio (0..1)
        double satCurve18V        = 2.2;        // tanh curve steepness

        // saturation characteristics — 9V supply
        double satThreshold9V     = 0.75;       // FS ratio
        double satCurve9V         = 3.2;        // tanh curve

        // Noise
        double inputNoiseDensity  = 8.0;        // Input-referred noise voltage density [nV/√Hz]
        double biasCurrentPA      = 0.065;      // input bias current [pA] (JFET) / [nA] (bipolar)
        double noiseScale         = 0.65;       // relative noise scale (TL072 reference = 0.65)

        // Temperature / tolerance
        double offsetDriftUvPerC  = 10.0;       // input offset drift [µV/°C]
        double gbwMismatchSigma   = 0.03;       // GBW manufacturing variance σ (±3% typ.)

        // Output stage asymmetry (PNP ratio)
        double asymNeg            = 0.92;       // negative-side saturation ratio (1.0 = symmetric)
    };

    // =====================================================================
    //  presets: Based on real device datasheet values
    // =====================================================================

    // TL072CP — Texas Instruments JFET input dual (1978–)
    // Low bias current (65pA), high slew rate (13V/µs), low noise (8nV/√Hz).
    // Bright, high-resolution tone. Standard for BBD delay, preamp, LFO buffer
    static constexpr Spec TL072CP()
    {
        return { "TL072CP", 13.0e6, 2e5, 3e6,
                 0.88, 2.2,  0.75, 3.2,
                 8.0, 0.065, 0.65,
                 10.0, 0.03, 0.92 };
    }

    // JRC4558D — NJR bipolar input dual (1976–)
    // Low slew rate (0.5V/µs) rolls off treble, creating dark warm repeat tone.
    // Original IC in classic overdrive pedal. Source of vintage overdrive 'sheen'
    static constexpr Spec JRC4558D()
    {
        return { "JRC4558D", 0.03e6, 5e4, 1e6,
                 0.58, 14.0,  0.48, 16.0,
                 5.0, 500.0, 2.2,
                 5.0, 0.08, 0.88 };
    }

    // NE5532 — Signetics/TI bipolar input dual (1979–)
    // Console/mixer standard. Low noise (5nV/√Hz), wide bandwidth (10MHz).
    // Punchy midrange, dense sound. Somewhat narrow headroom
    static constexpr Spec NE5532()
    {
        return { "NE5532", 9.0e6, 1e5, 10e6,
                 0.80, 4.5,  0.67, 5.5,
                 5.0, 200.0, 0.80,
                 5.0, 0.04, 0.90 };
    }

    // OPA2134 — Burr-Brown/TI JFET input dual (1996–)
    // Low distortion (0.00008%), wide bandwidth (8MHz), low noise. Hi-Fi clean.
    // High headroom, resistant to clipping, transparent tone
    static constexpr Spec OPA2134()
    {
        return { "OPA2134", 20.0e6, 5e5, 8e6,
                 0.95, 1.0,  0.84, 1.3,
                 8.0, 0.005, 0.22,
                 2.0, 0.02, 0.95 };
    }

    // LM4562 — National/TI bipolar input dual (2005–)
    // Ultra-low noise (2.7nV/√Hz), ultra-wideband (55MHz), THD+N=-140dB.
    // Standard 'cleanest upgrade' for studio equipment op-amp swaps
    static constexpr Spec LM4562()
    {
        return { "LM4562", 20.0e6, 1e6, 55e6,
                 0.97, 0.5,  0.90, 0.7,
                 2.7, 1.8, 0.08,
                 1.0, 0.01, 0.97 };
    }

    // LM741 — Fairchild/TI bipolar input single (1968–)
    // First general-purpose op-amp IC. Low slew rate (0.5V/µs), narrow bandwidth (1MHz),
    // high offset drift (15µV/°C). Bandwidth limiting and saturation create distinctive 'rawness.'
    // Needed to reproduce vintage fuzz and early effects tone
    static constexpr Spec LM741()
    {
        return { "LM741", 0.5e6, 2e5, 1e6,
                 0.72, 6.0,  0.55, 8.0,
                 20.0, 80.0, 3.5,
                 15.0, 0.10, 0.85 };
    }

    // =====================================================================
    //  constructor
    // =====================================================================

    OpAmpPrimitive() noexcept = default;

    explicit OpAmpPrimitive(const Spec& s, unsigned int seed = 0) noexcept
        : spec(s)
    {
        if (seed != 0)
        {
            std::minstd_rand initRng(seed);
            std::normal_distribution<double> tol(1.0, s.gbwMismatchSigma);
            gbwMismatch = std::clamp(tol(initRng), 0.85, 1.15);
        }
    }

    // =====================================================================
    //  initialization
    // =====================================================================

    void prepare(double sampleRate) noexcept
    {
        sr = (sampleRate > 1.0) ? sampleRate : 44100.0;
        dt = 1.0 / sr;
        updateSlewLimit();
    }

    void reset() noexcept
    {
        slewState = 0.0;
        driftState = 0.0;
    }

    // =====================================================================
    //  Processing methods
    // =====================================================================

    // integrated processing: slew rate limiting → saturation → offset drift
    inline double process(double x, bool highVoltage = true) noexcept
    {
        x = applySlewLimit(x);
        x = saturate(x, highVoltage);
        return x;
    }

    // Soft saturation (supply voltage dependent)
    // highVoltage = true → 18V operation, false → 9V operation
    inline double saturate(double x, bool highVoltage = true) const noexcept
    {
        double threshold = highVoltage ? spec.satThreshold18V : spec.satThreshold9V;
        double curve     = highVoltage ? spec.satCurve18V     : spec.satCurve9V;

        double absX = std::abs(x);
        if (absX <= threshold)
            return x;

        double sign = (x >= 0.0) ? 1.0 : -1.0;
        double negScale = (x < 0.0) ? spec.asymNeg : 1.0;
        double excess = absX - threshold;
        double compressed = threshold + std::tanh(excess * curve) * (1.0 - threshold);
        return sign * compressed * negScale;
    }

    // slew rate limiting (stateful — requires prepare + reset)
    inline double applySlewLimit(double x) noexcept
    {
        double delta = x - slewState;
        if (std::abs(delta) > slewLimitPerSample)
            x = slewState + (delta > 0.0 ? slewLimitPerSample : -slewLimitPerSample);
        slewState = x;
        return x;
    }

    // GBW-based effective bandwidth (-3dB frequency at closed-loop gain ACL)
    // f_3dB = GBW / ACL
    double bandwidthHz(double closedLoopGain = 1.0) const noexcept
    {
        double acl = std::max(closedLoopGain, 1.0);
        return (spec.gbwHz * gbwMismatch) / acl;
    }

    // temperature-dependent offset drift [V]
    // ΔVos = offsetDrift [µV/°C] × ΔT × 1e-6
    double offsetVoltage(double temperature = 25.0) const noexcept
    {
        double deltaT = temperature - 25.0;
        return spec.offsetDriftUvPerC * deltaT * 1e-6;
    }

    // Input-referred noise voltage (RMS) [V]
    // en = noiseDensity [nV/√Hz] × √(bandwidth) × 1e-9
    double inputNoiseVrms(double bandwidthHz_val = 20000.0) const noexcept
    {
        return spec.inputNoiseDensity * std::sqrt(std::max(bandwidthHz_val, 1.0)) * 1e-9;
    }

    // max sine frequency from slew rate (full-power bandwidth)
    // fmax = slewRate / (2π × Vpeak)
    double fullPowerBandwidthHz(double vPeak = 8.0) const noexcept
    {
        constexpr double kTwoPi = 6.283185307179586;
        return spec.slewRate / (kTwoPi * std::max(vPeak, 0.01));
    }

    // =====================================================================
    //  accessors
    // =====================================================================

    const Spec& getSpec() const noexcept { return spec; }
    double getGbwMismatch() const noexcept { return gbwMismatch; }

private:
    static constexpr double kOpAmpPeakVolt = 8.0;  // ±8V peak at 18V supply

    void updateSlewLimit() noexcept
    {
        slewLimitPerSample = spec.slewRate * dt / kOpAmpPeakVolt;
    }

    Spec spec     = TL072CP();
    double sr     = 44100.0;
    double dt     = 1.0 / 44100.0;
    double gbwMismatch       = 1.0;
    double slewState         = 0.0;
    double driftState        = 0.0;
    double slewLimitPerSample = 100.0;
};
