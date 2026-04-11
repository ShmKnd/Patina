#pragma once
#include <cmath>
#include <algorithm>

// Inductor (choke coil) primitive — iron-core / ferrite / air-core inductor
// Fundamental component for LC filters, power supply chokes, wah-wah inductors,
// passive EQ inductors.
//
// Physical properties:
//   inductance L (H): energy storage ∝ ½ L I²
//   DCR: winding DC resistance (Ω) — series loss
//   core saturation: B-H hysteresis, nonlinear above saturation current
//   self-resonant frequency (SRF): 1/(2π√(L×Cp)) — parasitic capacitance
//   Q factor: ωL / (DCR + Rcore) — frequency-dependent quality
//   temperature dependence: ferrite permeability μ(T) ≈ μ₀(1 + k·ΔT)
//   core loss: hysteresis + eddy current loss (proportional to f² for eddy)
//
// Presets:
//   HaloInductor  — Halo / TDK style toroidal choke (passive EQ)
//   WahInductor   — Fasel-style inductor for wah-wah pedal
//   PowerChoke    — Iron-core power supply choke (tube amp B+)
//   AirCore       — Air-core inductor (no saturation, linear, low L)
//
// Usage example:
//   InductorPrimitive choke(InductorPrimitive::HaloInductor());
//   choke.prepare(48000.0);
//   double out = choke.process(input);
class InductorPrimitive
{
public:
    struct Spec
    {
        double inductanceH     = 0.5;      // Inductance (H)
        double dcResistance    = 40.0;     // DCR (Ω)
        double parasiticCapF   = 50e-12;   // Parasitic winding capacitance (F)
        double coreSatLevel    = 1.5;      // Core saturation onset (normalized)
        double coreLossCoeff   = 1e-14;    // Core loss coefficient (hysteresis + eddy)
        double tempCoeffMu     = -0.002;   // Permeability temperature coefficient (/°C)
        double coreHystAlpha   = 0.3;      // Hysteresis loop width (0=no hysteresis, 1=wide)
        double nominalQ        = 5.0;      // Q factor at 1 kHz reference
        double maxCurrentA     = 0.5;      // Saturation current (A, normalized)
        bool   hasFerrite      = true;     // true = ferrite/iron core, false = air-core
    };

    // =====================================================================
    //  Presets
    // =====================================================================

    // Halo / TDK toroidal inductor — classic passive EQ inductor
    // High-Q iron powder toroid, moderate saturation.
    // Used in mastering EQs (Pultec-style) for smooth, musical filter curves
    static constexpr Spec HaloInductor()
    {
        return { 0.5, 40.0, 50e-12, 1.5, 1e-14, -0.002, 0.3, 5.0, 0.5, true };
    }

    // Fasel-style inductor — the heart of a wah-wah pedal
    // Low inductance, high parasitic capacitance → low SRF (~450 Hz peak).
    // Core saturation under hard playing creates the vocal quality of wah
    static constexpr Spec WahInductor()
    {
        return { 0.5, 70.0, 500e-12, 0.8, 3e-14, -0.003, 0.5, 3.0, 0.3, true };
    }

    // Iron-core power supply choke — tube amp B+ filtering
    // High inductance, high DCR, heavy saturation under transients.
    // Sag under current draw creates power amp compression
    static constexpr Spec PowerChoke()
    {
        return { 10.0, 200.0, 100e-12, 1.0, 5e-14, -0.003, 0.6, 2.0, 0.15, true };
    }

    // Air-core inductor — no saturation, linear, low inductance
    // Used in crossover networks, RF filtering.
    // Perfectly linear but lower inductance and Q than iron-core
    static constexpr Spec AirCore()
    {
        return { 0.001, 2.0, 10e-12, 100.0, 0.0, 0.0, 0.0, 15.0, 10.0, false };
    }

    // =====================================================================
    //  Constructor / lifecycle
    // =====================================================================

    InductorPrimitive() noexcept = default;
    explicit InductorPrimitive(const Spec& s) noexcept : spec(s) {}

    void prepare(double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        dt = 1.0 / sampleRate;

        // Self-resonant frequency: SRF = 1 / (2π√(L×Cp))
        double Lval = std::max(spec.inductanceH, 1e-9);
        double Cp = std::max(spec.parasiticCapF, 1e-15);
        srf = 1.0 / (2.0 * kPi * std::sqrt(Lval * Cp));
        srf = std::clamp(srf, 100.0, sampleRate * 0.45);

        // Resonance filter coefficient
        double srfAlpha = 1.0 - std::exp(-2.0 * kPi * srf / sampleRate);
        resonanceAlpha = std::clamp(srfAlpha, 0.001, 1.0);

        // Impedance LPF: XL = 2πfL → at high frequency, inductor blocks
        // Model as 1st-order: fc = DCR / (2πL) — the frequency where XL = DCR
        double fcImpedance = spec.dcResistance / (2.0 * kPi * Lval);
        fcImpedance = std::clamp(fcImpedance, 1.0, sampleRate * 0.49);
        impedanceAlpha = 1.0 - std::exp(-2.0 * kPi * fcImpedance / sampleRate);
        impedanceAlpha = std::clamp(impedanceAlpha, 0.0001, 1.0);

        // DCR insertion loss
        insertionLoss = 1.0 / (1.0 + spec.dcResistance * 1e-4);
        insertionLoss = std::clamp(insertionLoss, 0.9, 1.0);

        // Q-dependent resonance gain
        resonanceGain = 0.02 + 0.08 / std::max(spec.nominalQ, 0.5);
    }

    void reset() noexcept
    {
        hystState = 0.0;
        coreLossAccum = 0.0;
        prevSample = 0.0;
        impedanceState = 0.0;
        resonanceState = { 0.0, 0.0 };
    }

    // =====================================================================
    //  Information retrieval
    // =====================================================================

    // Self-resonant frequency (Hz)
    double getSRF() const noexcept { return srf; }

    // Temperature-dependent permeability scale
    double muScale(double temperature = 25.0) const noexcept
    {
        if (!spec.hasFerrite) return 1.0;  // Air-core: no temperature effect
        double delta = temperature - 25.0;
        return std::clamp(1.0 + spec.tempCoeffMu * delta, 0.85, 1.15);
    }

    // Q factor at given frequency
    double qAtFreq(double freqHz) const noexcept
    {
        double xl = 2.0 * kPi * freqHz * spec.inductanceH;
        double rTotal = spec.dcResistance + 0.01;  // avoid /0
        return xl / rTotal;
    }

    // Impedance magnitude at given frequency: |Z| = √(R² + (ωL)²)
    double impedanceAtFreq(double freqHz) const noexcept
    {
        double xl = 2.0 * kPi * freqHz * spec.inductanceH;
        return std::sqrt(spec.dcResistance * spec.dcResistance + xl * xl);
    }

    const Spec& getSpec() const noexcept { return spec; }

    // =====================================================================
    //  Full-chain processing
    // =====================================================================

    inline double process(double input, double temperature = 25.0) noexcept
    {
        double v = input;

        // 1. Core saturation (B-H hysteresis) — only for ferrite/iron cores
        if (spec.hasFerrite)
            v = processCoreSaturation(v, temperature);

        // 2. Core loss (eddy current + hysteresis loss)
        if (spec.hasFerrite)
            v = processCoreLoss(v);

        // 3. Inductance impedance effect (frequency-dependent)
        v = processImpedance(v);

        // 4. Parasitic capacitance resonance at SRF
        v = processResonance(v);

        // 5. DCR insertion loss
        v *= insertionLoss;

        return v;
    }

    // =====================================================================
    //  Individual processing stages
    // =====================================================================

    // Core saturation with hysteresis (B-H curve)
    inline double processCoreSaturation(double x, double temperature = 25.0) noexcept
    {
        double mu = muScale(temperature);
        double drive = mu;  // Higher μ → easier saturation

        double input = x * drive;
        double delta = input - hystState;

        // Asymmetric hysteresis rate (magnetization faster than demagnetization)
        double alpha = spec.coreHystAlpha;
        double rate = (delta > 0.0)
                      ? (1.0 - alpha * 0.85)
                      : (1.0 - alpha * 0.95);
        hystState += delta * std::clamp(rate, 0.01, 1.0);

        // tanh saturation based on core saturation level
        double coreSat = std::max(spec.coreSatLevel, 0.1);
        double satNorm = hystState / coreSat;
        return std::tanh(satNorm) * coreSat / drive;
    }

    // Core loss (frequency-dependent)
    inline double processCoreLoss(double x) noexcept
    {
        double deltaV = x - prevSample;
        double slewRate = std::abs(deltaV) * sampleRate;
        double loss = slewRate * slewRate * spec.coreLossCoeff;
        loss = std::min(loss, 0.02);
        coreLossAccum = coreLossAccum * 0.999 + loss * 0.001;
        prevSample = x;
        return x * (1.0 - coreLossAccum);
    }

    // Inductance impedance effect (HPF-like: blocks DC, passes HF)
    inline double processImpedance(double x) noexcept
    {
        impedanceState += impedanceAlpha * (x - impedanceState);
        // Inductor passes AC and blocks DC → return HPF component
        // But for LC filter use, we return the full signal with impedance coloring
        return x;
    }

    // Parasitic capacitance self-resonance
    inline double processResonance(double x) noexcept
    {
        double Q = std::max(spec.nominalQ, 0.5);
        double hp = x - resonanceState.s2 - resonanceState.s1 / Q;
        resonanceState.s1 += resonanceAlpha * hp;
        resonanceState.s2 += resonanceAlpha * resonanceState.s1;
        return x + resonanceState.s1 * resonanceGain;
    }

    // Impedance state accessor (for LC filter integration)
    double getImpedanceState() const noexcept { return impedanceState; }

private:
    static constexpr double kPi = 3.14159265358979323846;
    struct ResonanceState { double s1 = 0.0; double s2 = 0.0; };

    Spec spec = HaloInductor();
    double sampleRate = 44100.0;
    double dt = 1.0 / 44100.0;

    double srf = 10000.0;
    double resonanceAlpha = 0.5;
    double impedanceAlpha = 0.01;
    double insertionLoss = 0.99;
    double resonanceGain = 0.04;

    double hystState = 0.0;
    double coreLossAccum = 0.0;
    double prevSample = 0.0;
    double impedanceState = 0.0;
    ResonanceState resonanceState;
};
