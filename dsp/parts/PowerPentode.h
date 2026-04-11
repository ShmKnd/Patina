#pragma once
#include <cmath>
#include <algorithm>

// Power pentode (beam tetrode) primitive — output tube for push-pull amplifiers
// Fundamental component for guitar/bass amp power stages and hi-fi tube amplifiers.
//
// Physical properties:
//   plate characteristic: pentode mode Ip ≈ K (Vg1 + Vg2/μ₂)^(3/2)
//   screen grid feedback: Ig2 current sharing, ultralinear tap
//   beam forming: space charge → even-order harmonics in class AB
//   crossover distortion: class AB bias point notch
//   plate dissipation: thermal runaway protection
//   cathode bias: self-bias drift and bypass capacitor
//   grid current: blocking distortion at large positive grid swing
//   screen current: pentode → triode transition with ultralinear connection
//
// Presets (based on real tube datasheets):
//   EL34     — British classic. Marshall, Hiwatt. Aggressive mids, 25W per tube
//   _6L6GC   — American classic. Fender, Mesa. Clean headroom, 30W per tube
//   KT88     — Premium beam tetrode. Hi-Fi, SVT bass. Massive headroom, 42W per tube
//   EL84     — Small power pentode. Vox AC30, class A. Early breakup, chimey character
//   _6V6GT   — Small American tube. Fender Deluxe. Sweet breakup, 14W per tube
//
// Usage example:
//   PowerPentode tube(PowerPentode::EL34());
//   tube.prepare(48000.0);
//   double out = tube.process(inputGrid, biasPoint);
class PowerPentode
{
public:
    struct Spec
    {
        const char* name          = "EL34";

        // --- Transfer characteristic ---
        double transconductanceK  = 11.0e-3;   // K coefficient in Ip = K(Vg+Vg2/μ₂)^n
        double exponentN          = 1.5;        // Plate law exponent (1.5 = ideal pentode)
        double screenMu           = 22.0;       // Screen grid amplification factor μ₂
        double maxPlateCurrentMa  = 100.0;      // Maximum plate current (mA)

        // --- Saturation / clipping ---
        double posClipOnset        = 0.75;      // Positive clip onset (normalized)
        double negClipOnset        = 0.60;      // Negative clip onset (asymmetric, cutoff)
        double posClipCurve        = 2.5;       // Positive saturation curve (tanh steepness)
        double negClipCurve        = 4.0;       // Negative saturation curve (sharper cutoff)
        double crossoverNotch      = 0.02;      // Class AB crossover distortion depth
        double asymmetry           = 0.92;      // Negative half asymmetry (< 1.0 → cutoff earlier)

        // --- Screen grid / ultralinear ---
        double ultralinearTap      = 0.0;       // 0.0 = pentode, 0.43 = ultralinear, 1.0 = triode
        double screenSagCoeff      = 0.05;      // Screen voltage sag under high current

        // --- Thermal / aging ---
        double plateDissipationW   = 25.0;      // Max plate dissipation (W)
        double thermalTimeConst    = 2.0;       // Thermal time constant (seconds)
        double cathodeEmissionMax  = 0.3;       // Maximum cathode emission degradation

        // --- Impedance ---
        double plateResistance     = 15000.0;   // Plate resistance rp (Ω) — pentode mode
        double outputImpedance     = 5000.0;    // Effective output impedance before OT (Ω)
    };

    // =====================================================================
    //  Presets — based on real tube datasheet parameters
    // =====================================================================

    // EL34 (6CA7) — Mullard/Philips. The British power tube.
    // 25W plate dissipation. Aggressive midrange push. Defines the Marshall/Hiwatt sound.
    // Even-order harmonics dominate in push-pull → "crunch" character.
    // Biased in class AB with pronounced crossover notch for grit
    static constexpr Spec EL34()
    {
        return { "EL34",
                 11.0e-3, 1.5, 22.0, 100.0,
                 0.75, 0.60, 2.5, 4.0, 0.02, 0.92,
                 0.0, 0.05,
                 25.0, 2.0, 0.3,
                 15000.0, 5000.0 };
    }

    // 6L6GC — RCA/GE. The American power tube.
    // 30W plate dissipation. Wide, clean headroom. Fender Twin, Mesa Boogie.
    // Beam-forming plates create tighter bass than true pentode.
    // Higher headroom before clipping → clean with scooped mids
    static constexpr Spec _6L6GC()
    {
        return { "6L6GC",
                 9.0e-3, 1.45, 25.0, 120.0,
                 0.82, 0.65, 2.0, 3.5, 0.015, 0.94,
                 0.0, 0.04,
                 30.0, 2.5, 0.25,
                 22000.0, 6000.0 };
    }

    // KT88 — GEC/Gold Lion. Premium beam tetrode.
    // 42W plate dissipation. Massive headroom, deep bass extension.
    // Hi-Fi amplifier standard (McIntosh, Dynaco). Also SVT bass amplifier.
    // Extremely linear in push-pull → lowest THD of power tube family
    static constexpr Spec KT88()
    {
        return { "KT88",
                 12.0e-3, 1.5, 20.0, 150.0,
                 0.88, 0.72, 1.8, 3.0, 0.01, 0.96,
                 0.0, 0.03,
                 42.0, 3.0, 0.2,
                 12000.0, 4000.0 };
    }

    // EL84 (6BQ5) — Mullard. Small power pentode.
    // 12W plate dissipation. Early, musical breakup. Vox AC30 class A push-pull.
    // Chimey, harmonically rich when overdriven.
    // Low headroom means the power stage contributes significant coloring
    static constexpr Spec EL84()
    {
        return { "EL84",
                 8.5e-3, 1.5, 19.0, 65.0,
                 0.68, 0.52, 3.0, 4.5, 0.03, 0.88,
                 0.0, 0.07,
                 12.0, 1.5, 0.35,
                 38000.0, 8000.0 };
    }

    // 6V6GT — RCA. Small American power tube.
    // 14W plate dissipation. Sweet, singing breakup character.
    // Fender Deluxe, Princeton. Country, blues standard.
    // Lower maximum current → compresses earlier and more musically
    static constexpr Spec _6V6GT()
    {
        return { "6V6GT",
                 7.0e-3, 1.45, 23.0, 55.0,
                 0.70, 0.55, 2.8, 4.2, 0.025, 0.90,
                 0.0, 0.06,
                 14.0, 1.5, 0.35,
                 52000.0, 10000.0 };
    }

    // =====================================================================
    //  Constructor / lifecycle
    // =====================================================================

    PowerPentode() noexcept = default;
    explicit PowerPentode(const Spec& s) noexcept : spec(s) {}

    void prepare(double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        dt = 1.0 / sampleRate;

        // Thermal smoothing (plate dissipation tracking)
        thermalAlpha = dt / (spec.thermalTimeConst + dt);
        thermalAlpha = std::clamp(thermalAlpha, 0.0001, 0.1);

        // Screen sag smoothing
        screenSagAlpha = 1.0 - std::exp(-2.0 * kPi * 5.0 / sampleRate);  // ~5 Hz time constant

        // Grid blocking recovery
        gridBlockAlpha = 1.0 - std::exp(-2.0 * kPi * 2.0 / sampleRate);  // ~2 Hz
    }

    void reset() noexcept
    {
        thermalAccum = 0.0;
        screenSagState = 0.0;
        gridBlockState = 0.0;
    }

    // =====================================================================
    //  Processing — single-ended plate characteristic
    // =====================================================================

    // Process grid voltage through pentode transfer function
    // gridVoltage: normalized input signal (-1 to +1 range)
    // biasPoint: operating point (0.0 = class A center, negative = colder bias)
    // Returns: plate current (normalized), representing signal after tube
    inline double process(double gridVoltage, double biasPoint = -0.3) noexcept
    {
        double vg = gridVoltage + biasPoint;

        // 1. Grid current / blocking distortion
        vg = processGridConduction(vg);

        // 2. Pentode transfer function: Ip = K(Vg + Vg2/μ₂)^n
        double ip = transferFunction(vg);

        // 3. Screen voltage sag
        ip = processScreenSag(ip);

        // 4. Ultralinear feedback (if enabled)
        if (spec.ultralinearTap > 0.01)
            ip = applyUltralinearFeedback(ip);

        // 5. Thermal compression
        ip = processThermalCompression(ip);

        return ip;
    }

    // =====================================================================
    //  Individual processing stages
    // =====================================================================

    // Pentode transfer function
    // Ip ≈ K × max(0, Vg + Vg2/μ₂)^n with asymmetric saturation
    inline double transferFunction(double vg) const noexcept
    {
        // Class AB crossover notch (dead zone near cutoff)
        if (std::abs(vg) < spec.crossoverNotch)
        {
            double t = vg / std::max(spec.crossoverNotch, 1e-6);
            vg = spec.crossoverNotch * t * t * t;  // Smooth cubic through zero
        }

        double sign = (vg >= 0.0) ? 1.0 : -1.0;
        double absVg = std::abs(vg);

        // Positive half: smooth compression
        if (vg >= 0.0)
        {
            if (absVg <= spec.posClipOnset)
            {
                // Normal operating region — pentode law
                return spec.transconductanceK * std::pow(absVg + 0.01, spec.exponentN) * 80.0;
            }
            else
            {
                // Soft saturation (grid current region)
                double excess = absVg - spec.posClipOnset;
                double linear = spec.transconductanceK * std::pow(spec.posClipOnset + 0.01, spec.exponentN) * 80.0;
                return linear + std::tanh(excess * spec.posClipCurve) * (1.0 - spec.posClipOnset) * 0.5;
            }
        }
        else
        {
            // Negative half: sharper cutoff (asymmetric)
            if (absVg <= spec.negClipOnset)
            {
                double ip = spec.transconductanceK * std::pow(absVg + 0.01, spec.exponentN) * 80.0;
                return -ip * spec.asymmetry;
            }
            else
            {
                double excess = absVg - spec.negClipOnset;
                double linear = spec.transconductanceK * std::pow(spec.negClipOnset + 0.01, spec.exponentN) * 80.0;
                double ip = linear + std::tanh(excess * spec.negClipCurve) * (1.0 - spec.negClipOnset) * 0.5;
                return -ip * spec.asymmetry;
            }
        }
    }

    // Grid conduction / blocking distortion
    inline double processGridConduction(double vg) noexcept
    {
        if (vg > 0.0)
        {
            // Grid current when Vg > 0 — charges coupling capacitor
            gridBlockState += gridBlockAlpha * vg * 0.5;
            gridBlockState = std::min(gridBlockState, 0.5);
        }
        else
        {
            // Slow discharge
            gridBlockState *= (1.0 - gridBlockAlpha * 0.1);
        }

        return vg - gridBlockState;
    }

    // Screen voltage sag (current-dependent)
    inline double processScreenSag(double ip) noexcept
    {
        double absIp = std::abs(ip);
        double sagTarget = absIp * spec.screenSagCoeff;
        screenSagState += screenSagAlpha * (sagTarget - screenSagState);
        return ip * (1.0 - std::clamp(screenSagState, 0.0, 0.3));
    }

    // Ultralinear feedback (screen tap of output transformer)
    // Reduces distortion, lowers output impedance toward triode
    inline double applyUltralinearFeedback(double ip) const noexcept
    {
        double tap = std::clamp(spec.ultralinearTap, 0.0, 1.0);
        // Negative feedback proportional to tap: reduces gain and distortion
        double feedback = ip * tap * 0.3;
        return ip / (1.0 + std::abs(feedback));
    }

    // Thermal compression (plate dissipation → gradual gain reduction)
    inline double processThermalCompression(double ip) noexcept
    {
        double instantPower = ip * ip;  // Normalized power
        thermalAccum += thermalAlpha * (instantPower - thermalAccum);
        thermalAccum = std::clamp(thermalAccum, 0.0, 1.0);

        // Compression increases with thermal accumulation
        double compression = 1.0 / (1.0 + thermalAccum * 0.3);
        return ip * compression;
    }

    // =====================================================================
    //  Accessors
    // =====================================================================

    const Spec& getSpec() const noexcept { return spec; }
    double getThermalState() const noexcept { return thermalAccum; }
    double getScreenSagState() const noexcept { return screenSagState; }

private:
    static constexpr double kPi = 3.14159265358979323846;

    Spec spec = EL34();
    double sampleRate = 44100.0;
    double dt = 1.0 / 44100.0;

    double thermalAlpha = 0.001;
    double screenSagAlpha = 0.01;
    double gridBlockAlpha = 0.01;

    double thermalAccum = 0.0;
    double screenSagState = 0.0;
    double gridBlockState = 0.0;
};
