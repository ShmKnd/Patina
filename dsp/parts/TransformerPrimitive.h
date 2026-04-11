#pragma once
#include <cmath>
#include <algorithm>

// Transformer primitive
// Comprehensive physical model of audio transformer — console, mic preamp, DI, interstage coupling, etc.
//
// Physical properties:
//   winding ratio: N = N₂/N₁ → voltage conversion Vout = Vin × N
//   impedance conversion: Zout = Zin × N²
//   primary inductance: Lp → LF lower frequency fc = Zsource / (2πLp)
//   Winding resistance: R_w → slight HF attenuation and insertion loss
//   leakage inductance: Lleak = (1−k)Lp → HF rolloff
//   winding capacitance: Cw → HF resonance peak fr = 1/(2π√(Lleak×Cw))
//   magnetic saturation: B-H hysteresis loop (asymmetric → even harmonics)
//   core loss: hysteresis loss + eddy current loss
//   temperature-dependent permeability: μ(T) = μ₀ × (1 + k_temp × ΔT)
//   CMRR: common-mode rejection on balanced input
//   phase inversion: signal inversion via winding polarity
//
// Usage example:
//   TransformerPrimitive xfmr(TransformerPrimitive::Neve1073Input());
//   xfmr.prepare(48000.0);
//   double out = xfmr.process(input);
//   double balOut = xfmr.processBalanced(hot, cold);
class TransformerPrimitive
{
public:
    struct Spec
    {
        // --- basic properties ---
        double windingR       = 600.0;     // Winding resistance (Ω)
        double leakageL       = 2e-3;      // leakage inductance (H)
        double windingCap     = 200e-12;   // winding capacitance (F)
        double coreLossCoeff  = 1e-14;     // core loss coefficient
        double tempCoeffMu    = -0.001;    // permeability temperature coefficient (/°C)
        double dcBiasScale    = 0.05;      // DC bias scale
        double resonanceGain  = 0.12;      // Winding resonance gain
        double noiseLevel     = 1e-6;      // winding thermal noise

        // --- extended properties ---
        double turnsRatio        = 1.0;      // N₂/N₁ winding ratio
        double primaryInductance = 10.0;     // primary inductance Lp (H)
        double couplingCoeff     = 0.998;    // coupling coefficient k (0–1)
        double primaryDCR        = 50.0;     // Primary DC resistance (Ω)
        double secondaryDCR      = 50.0;     // Secondary DC resistance (Ω)
        double nominalSourceZ    = 150.0;    // design target source impedance (Ω)
        double coreSatLevel      = 1.2;      // core saturation onset level
        double cmrr_dB           = 80.0;     // common-mode rejection ratio (dB)
        double resonanceQ        = 2.5;      // HF resonance Q value
        bool   invertPhase       = false;    // phase inversion
    };

    // =====================================================================
    //  Console / output stage presets
    // =====================================================================

    // British console type — characteristic of British consoles represented by 1073-series
    // transformers. Bass thickness and even harmonics from magnetic saturation define the British console sound
    static constexpr Spec BritishConsole()
    {
        return { 600.0, 2e-3, 200e-12, 1e-14, -0.001, 0.05, 0.12, 1e-6,
                 1.0, 10.0, 0.995, 50.0, 50.0, 150.0, 1.2, 80.0, 2.5, false };
    }
    // American console type — tight transformer used in 500-series modules.
    // Tighter, punchier midrange than British type. Standard for rock drum recording
    static constexpr Spec AmericanConsole()
    {
        return { 400.0, 1.5e-3, 150e-12, 8e-15, -0.0008, 0.04, 0.10, 8e-7,
                 1.0, 5.0, 0.998, 30.0, 30.0, 100.0, 1.5, 80.0, 3.0, false };
    }
    // FET compressor output transformer — legendary FET limiter/
    // compressor output stage. Light saturation from compact transformer adds sheen to mix
    static constexpr Spec CompactFetOutput()
    {
        return { 300.0, 1e-3, 100e-12, 5e-15, -0.001, 0.03, 0.08, 5e-7,
                 1.0, 3.0, 0.998, 20.0, 20.0, 100.0, 1.5, 80.0, 3.0, false };
    }

    // =====================================================================
    //  Mic preamp / DI / interstage coupling presets
    // =====================================================================

    // British mic preamp 1073 input transformer — symbol of 1970s recording studios.
    // 1:10 step-up boosts ribbon mic signal; nickel-alloy core
    // saturation creates distinctive 'silk' via even harmonics on vocals and drums
    static constexpr Spec Neve1073Input()
    {
        return { 600.0, 2e-3, 200e-12, 1.5e-14, -0.001, 0.05, 0.12, 1e-6,
                 10.0, 10.0, 0.995, 50.0, 5000.0, 150.0, 1.2, 90.0, 2.5, false };
    }
    // American console 2520 output transformer — standard output stage.
    // tight 1:1 design features punchy midrange and clear transients
    static constexpr Spec API2520Output()
    {
        return { 400.0, 1.5e-3, 100e-12, 8e-15, -0.0008, 0.04, 0.10, 5e-7,
                 1.0, 5.0, 0.998, 30.0, 30.0, 100.0, 1.5, 80.0, 3.0, false };
    }
    // Lundahl LL1538 — Swedish modern high-end mic preamp input transformer
    static constexpr Spec Lundahl1538()
    {
        return { 400.0, 1.5e-3, 100e-12, 5e-15, -0.0005, 0.03, 0.08, 3e-7,
                 5.0, 15.0, 0.998, 25.0, 600.0, 150.0, 1.8, 100.0, 4.0, false };
    }
    // DI box transformer — converts unbalanced Hi-Z guitar/bass signal
    // to balanced Lo-Z line via isolation transformer
    static constexpr Spec JensenDIBox()
    {
        return { 200.0, 1e-3, 500e-12, 2e-14, -0.001, 0.05, 0.06, 2e-6,
                 0.1, 140.0, 0.990, 200.0, 2.0, 50000.0, 0.8, 70.0, 1.5, false };
    }
    // Tube interstage coupling transformer — provides phase inversion while DC-isolating,
    // driving push-pull output stage
    static constexpr Spec InterstageTriode()
    {
        return { 300.0, 1e-3, 350e-12, 2e-14, -0.002, 0.05, 0.08, 2e-6,
                 1.0, 150.0, 0.990, 300.0, 300.0, 50000.0, 0.8, 60.0, 2.0, true };
    }
    // Ribbon mic output transformer — ultra-high turns ratio converting
    // low-impedance ribbon element (~0.2Ω) to mic preamp input
    static constexpr Spec RibbonMicOutput()
    {
        return { 100.0, 1e-3, 300e-12, 3e-14, -0.001, 0.05, 0.10, 3e-6,
                 35.0, 0.5, 0.985, 0.5, 600.0, 0.2, 0.6, 50.0, 2.0, false };
    }

    TransformerPrimitive() noexcept = default;
    explicit TransformerPrimitive(const Spec& s) noexcept : spec(s) {}

    void prepare(double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);

        // LF coupling HP corner: fc = Zsource / (2π × Lp)
        double fcLf = spec.nominalSourceZ
                      / (2.0 * kPi * std::max(spec.primaryInductance, 1e-6));
        lfHpAlpha = 1.0 - std::exp(-2.0 * kPi * fcLf / sampleRate);
        lfHpAlpha = std::clamp(lfHpAlpha, 0.0001, 0.5);

        // HF rolloff from leakage inductance
        double Lleak = (1.0 - spec.couplingCoeff) * spec.primaryInductance;
        Lleak = std::max(Lleak, 1e-9);
        double Cw = std::max(spec.windingCap, 1e-15);
        fullResonanceFreq = 1.0 / (2.0 * kPi * std::sqrt(Lleak * Cw));
        fullResonanceFreq = std::clamp(fullResonanceFreq, 1000.0, sampleRate * 0.45);
        double frAlpha = 1.0 - std::exp(-2.0 * kPi * fullResonanceFreq / sampleRate);
        fullResonanceAlpha = std::clamp(frAlpha, 0.001, 1.0);

        double fcHf = fullResonanceFreq * 1.5;
        hfLpAlpha = 1.0 - std::exp(-2.0 * kPi * fcHf / sampleRate);
        hfLpAlpha = std::clamp(hfLpAlpha, 0.01, 1.0);

        // CMRR linear scale
        cmrrLinear = std::pow(10.0, -spec.cmrr_dB / 20.0);

        // insertion loss (loss via DCR)
        double rReflected = spec.secondaryDCR
                            / std::max(spec.turnsRatio * spec.turnsRatio, 1e-6);
        insertionLoss = 1.0 / (1.0 + (spec.primaryDCR + rReflected) * 1e-4);
        insertionLoss = std::clamp(insertionLoss, 0.9, 1.0);
    }

    void reset() noexcept
    {
        fullHystState = 0.0;
        fullPrevSample = 0.0;
        fullCoreLossAccum = 0.0;
        lfHpState = 0.0;
        hfLpState = 0.0;
        fullResonanceState = { 0.0, 0.0 };
    }

    // =====================================================================
    //  information retrieval
    // =====================================================================

    // temperature-dependent permeability scale
    double muScale(double temperature = 25.0) const noexcept
    {
        double delta = temperature - 25.0;
        return std::clamp(1.0 + spec.tempCoeffMu * delta, 0.90, 1.10);
    }

    // impedance conversion ratio (N²)
    double impedanceRatio() const noexcept
    {
        return spec.turnsRatio * spec.turnsRatio;
    }

    // HF resonance frequency (from full-chain leakage inductance)
    double getResonanceFreq() const noexcept { return fullResonanceFreq; }

    const Spec& getSpec() const noexcept { return spec; }

    // =====================================================================
    //  full-chain processing
    // =====================================================================

    // single-ended input
    inline double process(double input,
                          double satAmount   = 0.3,
                          double temperature = 25.0) noexcept
    {
        double v = input;

        // 1. LF coupling (HP from primary inductance)
        v = processLfCoupling(v);

        // 2. magnetic core saturation (B-H hysteresis)
        double mu = muScale(temperature);
        v = processCoreSaturation(v, satAmount, mu);

        // 3. core loss (eddy current + hysteresis loss)
        v = processCoreLoss(v);

        // 4. voltage conversion via winding ratio
        v *= spec.turnsRatio;

        // 5. winding capacitance resonance
        v = processResonance(v);

        // 6. leakage inductance HF rolloff
        v = processHfRolloff(v);

        // 7. insertion loss
        v *= insertionLoss;

        // 8. phase inversion
        if (spec.invertPhase) v = -v;

        return v;
    }

    // balanced input processing (differential extraction + CMRR)
    inline double processBalanced(double hot, double cold,
                                  double satAmount   = 0.3,
                                  double temperature = 25.0) noexcept
    {
        double differential = hot - cold;
        double commonMode   = (hot + cold) * 0.5;

        double out = process(differential, satAmount, temperature);

        // common-mode leakage addition
        out += commonMode * cmrrLinear * spec.turnsRatio;

        return out;
    }

    // =====================================================================
    //  individual processing methods
    // =====================================================================

    // LF coupling (high-pass filter from primary inductance)
    inline double processLfCoupling(double x) noexcept
    {
        lfHpState += lfHpAlpha * (x - lfHpState);
        return x - lfHpState;
    }

    // magnetic core saturation (asymmetric hysteresis → even harmonics)
    inline double processCoreSaturation(double x,
                                        double satAmount,
                                        double muScl = 1.0) noexcept
    {
        double drive = (1.0 + satAmount * 2.0) * muScl;
        double input = x * drive;
        double delta = input - fullHystState;

        // asymmetric rate: speed difference positive→negative → even harmonics
        // magnetization (rise) faster than demagnetization (fall) — ~2:1 physically reasonable
        // alpha set low to prevent audio-band tracking lag (level drop)
        // saturation character handled primarily by tanh nonlinearity
        double alpha = 0.3 - satAmount * 0.1;
        double rate  = (delta > 0.0)
                       ? (1.0 - alpha * 0.85)
                       : (1.0 - alpha * 0.95);
        fullHystState += delta * std::clamp(rate, 0.01, 1.0);

        // tanh clipping based on core saturation level
        double coreSat = std::max(spec.coreSatLevel, 0.1);
        double satNorm = fullHystState / coreSat;
        return std::tanh(satNorm) * coreSat / drive;
    }

    // core loss
    inline double processCoreLoss(double x) noexcept
    {
        double deltaV   = x - fullPrevSample;
        double slewRate = std::abs(deltaV) * sampleRate;
        double loss     = slewRate * slewRate * spec.coreLossCoeff;
        loss = std::min(loss, 0.02);
        fullCoreLossAccum = fullCoreLossAccum * 0.999 + loss * 0.001;
        fullPrevSample = x;
        return x * (1.0 - fullCoreLossAccum);
    }

    // winding capacitance resonance
    inline double processResonance(double x) noexcept
    {
        double Q  = std::max(spec.resonanceQ, 0.5);
        double hp = x - fullResonanceState.s2 - fullResonanceState.s1 / Q;
        fullResonanceState.s1 += fullResonanceAlpha * hp;
        fullResonanceState.s2 += fullResonanceAlpha * fullResonanceState.s1;
        double resGain = 0.05 + 0.15 / Q;
        return x + fullResonanceState.s1 * resGain;
    }

    // HF rolloff (from leakage inductance)
    inline double processHfRolloff(double x) noexcept
    {
        hfLpState += hfLpAlpha * (x - hfLpState);
        return hfLpState;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    struct ResonanceState { double s1 = 0.0; double s2 = 0.0; };

    Spec spec = BritishConsole();
    double sampleRate = 44100.0;

    // --- Model state ---
    double lfHpAlpha      = 0.001;
    double hfLpAlpha      = 0.5;
    double fullResonanceFreq  = 8000.0;
    double fullResonanceAlpha = 0.5;
    double cmrrLinear     = 0.0001;
    double insertionLoss  = 0.99;
    double fullHystState     = 0.0;
    double fullPrevSample    = 0.0;
    double fullCoreLossAccum = 0.0;
    double lfHpState     = 0.0;
    double hfLpState     = 0.0;
    ResonanceState fullResonanceState;
};
