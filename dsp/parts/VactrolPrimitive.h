#pragma once
#include <cmath>
#include <algorithm>

// Vactrol (LED + LDR) primitive — optocoupler for voltage-controlled resistance
// A modern alternative to photocell (EL+CdS): LED illuminates CdS/CdSe photoresistor.
// Used for tremolo, auto-wah, optical compressor, voltage-controlled filter,
// and modular synth VCA/VCF applications.
//
// Physical properties:
//   LED: fast response (< 1μs), linear brightness vs current
//   LDR (CdS/CdSe): photoresistance varies ~10MΩ (dark) to ~100Ω (bright)
//     attack: 0.5–5ms (dark→bright, crystal lattice activation)
//     release: 5ms–500ms+ (bright→dark, crystal lattice relaxation)
//     memory effect: prolonged illumination → slower release (crystal structure change)
//     nonlinear resistance curve: R ∝ E^(-γ), γ ≈ 0.7–0.9
//   Distortion: at low resistance, LDR exhibits slight nonlinearity
//   Temperature: CdS sensitivity decreases at high temperature
//
// Key difference from PhotocellPrimitive:
//   PhotocellPrimitive models EL panel (electroluminescent) + CdS
//   VactrolPrimitive models LED + CdS/CdSe (modern, faster, more controllable)
//
// Presets:
//   VTL5C3    — PerkinElmer. Fast, medium memory. Modular synth standard
//   VTL5C1    — PerkinElmer. Slower, heavier memory. Tremolo / compressor
//   NSL32SR3  — Silonex. Ultra-fast, low memory. Precision VCA applications
//   Custom    — DIY: discrete LED + LDR combo (wide variation)
//
// Usage example:
//   VactrolPrimitive vactrol(VactrolPrimitive::VTL5C3());
//   vactrol.prepare(48000.0);
//   double resistance = vactrol.process(controlVoltage);
//   // resistance: 0.0 (dark, max R) → 1.0 (bright, min R)
class VactrolPrimitive
{
public:
    struct Spec
    {
        const char* name         = "VTL5C3";

        // --- LED stage ---
        double ledAttackMs       = 0.01;    // LED rise time (ms) — nearly instant
        double ledReleaseMs      = 0.05;    // LED fall time (ms)

        // --- LDR stage ---
        double ldrAttackMs       = 1.0;     // LDR attack (dark→bright) (ms)
        double ldrReleaseMinMs   = 10.0;    // LDR minimum release (ms)
        double ldrReleaseMaxMs   = 500.0;   // LDR maximum release (with full memory) (ms)
        double ldrGamma          = 0.8;     // Resistance curve nonlinearity exponent
        double ldrMinResistance  = 100.0;   // Minimum resistance (Ω) at full illumination
        double ldrMaxResistance  = 10e6;    // Maximum resistance (Ω) in dark

        // --- Memory effect ---
        double memoryChargeRate  = 0.8;     // Memory charge rate (/s)
        double memoryDischargeRate = 0.15;  // Memory discharge rate (/s)

        // --- Nonlinearity ---
        double distortionCoeff   = 0.003;   // Low-resistance distortion coefficient
        double tempCoeffSensitivity = -0.005; // Temperature sensitivity coefficient (/°C)
    };

    // =====================================================================
    //  Presets
    // =====================================================================

    // VTL5C3 — PerkinElmer. Fast response, medium memory.
    // Standard modular synth vactrol. Tremolo, VCF, envelope follower.
    // Good balance of speed and musicality
    static constexpr Spec VTL5C3()
    {
        return { "VTL5C3",
                 0.01, 0.05,
                 1.0, 10.0, 500.0, 0.8, 100.0, 10e6,
                 0.8, 0.15,
                 0.003, -0.005 };
    }

    // VTL5C1 — PerkinElmer. Slower, heavier memory effect.
    // Natural tremolo modulation. Optical compressor side-chain.
    // Release shape varies with signal history → musical compression
    static constexpr Spec VTL5C1()
    {
        return { "VTL5C1",
                 0.01, 0.05,
                 2.0, 20.0, 800.0, 0.85, 150.0, 20e6,
                 1.2, 0.08,
                 0.005, -0.006 };
    }

    // NSL-32SR3 — Silonex. Ultra-fast, low memory.
    // Precision VCA, sample-and-hold, gating applications.
    // Minimal coloring, clean gain control
    static constexpr Spec NSL32SR3()
    {
        return { "NSL-32SR3",
                 0.005, 0.02,
                 0.5, 5.0, 100.0, 0.75, 80.0, 5e6,
                 0.3, 0.3,
                 0.001, -0.004 };
    }

    // DIY: discrete LED + LDR combo
    // Wide part-to-part variation, imprecise but characterful.
    // Each build unique — typical of boutique pedal builders
    static constexpr Spec DIY_Custom()
    {
        return { "DIY",
                 0.02, 0.1,
                 3.0, 30.0, 1000.0, 0.9, 200.0, 50e6,
                 1.5, 0.05,
                 0.008, -0.008 };
    }

    // =====================================================================
    //  Constructor / lifecycle
    // =====================================================================

    VactrolPrimitive() noexcept = default;
    explicit VactrolPrimitive(const Spec& s) noexcept : spec(s) {}

    void prepare(double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
    }

    void reset() noexcept
    {
        ledBrightness = 0.0;
        ldrResponse = 0.0;
        memoryAccum = 0.0;
    }

    // =====================================================================
    //  Processing
    // =====================================================================

    // Process control voltage → returns normalized conductance (0=dark/high-R, 1=bright/low-R)
    // controlVoltage: 0.0 = off, 1.0 = full brightness
    inline double process(double controlVoltage) noexcept
    {
        double cv = std::clamp(controlVoltage, 0.0, 1.0);

        // 1. LED stage — nearly instantaneous
        double ledAtt = msToAlpha(spec.ledAttackMs);
        double ledRel = msToAlpha(spec.ledReleaseMs);

        if (cv > ledBrightness)
            ledBrightness += ledAtt * (cv - ledBrightness);
        else
            ledBrightness += ledRel * (cv - ledBrightness);

        ledBrightness = std::clamp(ledBrightness, 0.0, 1.0);

        // 2. Memory effect — prolonged illumination slows release
        if (ledBrightness > 0.1)
            memoryAccum += (1.0 - memoryAccum) * spec.memoryChargeRate / sampleRate;
        else
            memoryAccum *= (1.0 - spec.memoryDischargeRate / sampleRate);

        memoryAccum = std::clamp(memoryAccum, 0.0, 1.0);

        // 3. LDR stage — asymmetric attack/release with memory
        double ldrAtt = msToAlpha(spec.ldrAttackMs);
        double releaseMs = spec.ldrReleaseMinMs
                         + memoryAccum * (spec.ldrReleaseMaxMs - spec.ldrReleaseMinMs);
        double ldrRel = msToAlpha(releaseMs);

        if (ledBrightness > ldrResponse)
            ldrResponse += ldrAtt * (ledBrightness - ldrResponse);
        else
            ldrResponse += ldrRel * (ledBrightness - ldrResponse);

        ldrResponse = std::clamp(ldrResponse, 0.0, 1.0);

        // 4. Nonlinear resistance curve: R ∝ E^(-γ)
        // Returns conductance (inverse of resistance, normalized 0-1)
        double conductance = std::pow(std::max(ldrResponse, 1e-10), spec.ldrGamma);

        return conductance;
    }

    // Process with temperature dependence
    inline double process(double controlVoltage, double temperature) noexcept
    {
        double result = process(controlVoltage);

        // Temperature: CdS sensitivity decreases at high temperature
        double tempDelta = temperature - 25.0;
        double tempScale = 1.0 + spec.tempCoeffSensitivity * tempDelta;
        tempScale = std::clamp(tempScale, 0.7, 1.3);

        return result * tempScale;
    }

    // Apply vactrol as a signal attenuator (for tremolo, VCA use)
    // input: audio signal, controlVoltage: 0-1 modulation depth
    // Returns: attenuated signal
    inline double applyAsAttenuator(double input, double controlVoltage) noexcept
    {
        double conductance = process(controlVoltage);

        // At low conductance (high R), add slight distortion
        double distortion = 0.0;
        if (conductance < 0.3)
        {
            distortion = spec.distortionCoeff * input * input * (0.3 - conductance);
        }

        return input * conductance + distortion;
    }

    // =====================================================================
    //  Information retrieval
    // =====================================================================

    // Get current resistance in ohms
    double getResistanceOhms() const noexcept
    {
        double conductance = std::pow(std::max(ldrResponse, 1e-10), spec.ldrGamma);
        if (conductance < 1e-10) return spec.ldrMaxResistance;
        double resistanceRange = spec.ldrMaxResistance - spec.ldrMinResistance;
        return spec.ldrMinResistance + resistanceRange * (1.0 - conductance);
    }

    // Get current LED brightness (0-1)
    double getLedBrightness() const noexcept { return ledBrightness; }

    // Get current memory accumulation (0-1)
    double getMemoryState() const noexcept { return memoryAccum; }

    // Get current LDR response (0-1)
    double getLdrResponse() const noexcept { return ldrResponse; }

    const Spec& getSpec() const noexcept { return spec; }

private:
    inline double msToAlpha(double ms) const noexcept
    {
        if (ms <= 0.0) return 1.0;
        return 1.0 - std::exp(-1.0 / (sampleRate * ms * 0.001));
    }

    Spec spec = VTL5C3();
    double sampleRate = 44100.0;

    double ledBrightness = 0.0;
    double ldrResponse = 0.0;
    double memoryAccum = 0.0;
};
