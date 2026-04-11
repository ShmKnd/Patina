#pragma once
#include <cmath>
#include <algorithm>

// Photocell (T4B) primitive
// Heart of optical compressor: EL panel + CdS photoresistor cell
// Memory effect of CdS — release lengthens with light exposure duration;
// generates natural compression
//
// Physical properties:
//   EL panel: voltage→brightness (fast response ~2ms)
//   CdS LDR: light-to-resistance conversion (asymmetric response)
//     attack ~10ms (dark→bright)
//     release ~60ms–5s (bright→dark, varies with memory effect)
//   memory effect: prolonged light exposure alters CdS crystal structure → slows release
//
// Usage example:
//   PhotocellPrimitive cell;
//   cell.prepare(44100.0);
//   double gain = cell.process(sidechainLevel);
class PhotocellPrimitive
{
public:
    struct Spec
    {
        double elAttackMs         = 2.0;    // EL panel attack (ms)
        double elReleaseMs        = 10.0;   // EL panel release (ms)
        double cdsAttackMs        = 10.0;   // CdS attack (ms)
        double cdsReleaseMinMs    = 60.0;   // CdS release minimum (ms)
        double cdsReleaseMaxMs    = 5000.0; // CdS release maximum — full memory effect (ms)
        double historyChargeRate  = 0.5;    // memory charge rate (/s)
        double historyDischargeRate = 0.05; // memory discharge rate (/s)
    };

    // T4B — EL + CdS photocell adopted in 1960s optical compressors.
    // Slow release and memory effect create 'musical compression' perfect for vocals and
    // bass dynamics — still unique today
    static constexpr Spec T4B() { return { 2.0, 10.0, 10.0, 60.0, 5000.0, 0.5, 0.05 }; }
    // VTL5C3 — modern LED + LDR vactrol. Faster response than T4B;
    // widely used as VCA element in DIY modular synths — tremolo, auto-wah,
    // envelope follower, etc.
    static constexpr Spec VTL5C3() { return { 0.5, 5.0, 5.0, 30.0, 2000.0, 0.3, 0.1 }; }

    PhotocellPrimitive() noexcept = default;
    explicit PhotocellPrimitive(const Spec& s) noexcept : spec(s) {}

    void prepare(double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
    }

    void reset() noexcept
    {
        elBrightness = 0.0;
        cdsResistanceInv = 0.0;
        historyAccum = 0.0;
    }

    // photocell processing: input level → attenuation (0=unity, high=large attenuation)
    inline double process(double scLevel) noexcept
    {
        // EL panel brightness tracking
        double elAtt = msToAlpha(spec.elAttackMs);
        double elRel = msToAlpha(spec.elReleaseMs);
        if (scLevel > elBrightness)
            elBrightness += elAtt * (scLevel - elBrightness);
        else
            elBrightness += elRel * (scLevel - elBrightness);

        // memory effect
        if (elBrightness > 0.1)
            historyAccum += (1.0 - historyAccum) * spec.historyChargeRate / sampleRate;
        else
            historyAccum *= (1.0 - spec.historyDischargeRate / sampleRate);
        historyAccum = std::clamp(historyAccum, 0.0, 1.0);

        // CdS response (asymmetric)
        double cdsAtt = msToAlpha(spec.cdsAttackMs);
        double releaseMs = spec.cdsReleaseMinMs
                         + historyAccum * (spec.cdsReleaseMaxMs - spec.cdsReleaseMinMs);
        double cdsRel = msToAlpha(releaseMs);

        if (elBrightness > cdsResistanceInv)
            cdsResistanceInv += cdsAtt * (elBrightness - cdsResistanceInv);
        else
            cdsResistanceInv += cdsRel * (elBrightness - cdsResistanceInv);

        return std::clamp(cdsResistanceInv, 0.0, 1.0);
    }

    const Spec& getSpec() const noexcept { return spec; }

private:
    inline double msToAlpha(double ms) const noexcept
    {
        if (ms <= 0.0) return 1.0;
        return 1.0 - std::exp(-1.0 / (sampleRate * ms * 0.001));
    }

    Spec spec = T4B();
    double sampleRate = 44100.0;
    double elBrightness = 0.0;
    double cdsResistanceInv = 0.0;
    double historyAccum = 0.0;
};
