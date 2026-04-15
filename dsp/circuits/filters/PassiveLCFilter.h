#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../core/FastMath.h"
#include "../../parts/InductorPrimitive.h"
#include "../../parts/RC_Element.h"

// =============================================================================
//  PassiveLCFilter — Passive LC (Inductor-Capacitor) Filter
//
//  Classic passive filter topology using physical inductor and capacitor elements.
//  Models the behavior of Pultec-style passive EQ, wah-wah pedal resonant filters,
//  and vintage passive crossover networks.
//
//  Topologies:
//    LPF:  Series inductor → shunt capacitor (2nd order, -12dB/oct)
//    HPF:  Series capacitor → shunt inductor (2nd order, -12dB/oct)
//    BPF:  Series LC → resonance at fr = 1/(2π√LC)
//    Notch: Parallel LC → anti-resonance
//
//  Physical characteristics:
//    - Inductor core saturation introduces harmonics at high levels
//    - Inductor DCR causes insertion loss
//    - Parasitic capacitance creates self-resonant frequency limit
//    - Temperature-dependent inductor permeability shifts tuning
//    - Q factor depends on component losses (DCR + core loss)
//
//  4-layer architecture:
//    Parts: InductorPrimitive × 1 + RC_Element × 1
//    → Circuit: PassiveLCFilter (passive 2nd-order filter with analog character)
//
//  Usage:
//    PassiveLCFilter filter;
//    filter.prepare(2, 48000.0);
//    filter.setCutoffHz(1000.0f);
//    float out = filter.process(0, input, PassiveLCFilter::LPF);
// =============================================================================
class PassiveLCFilter
{
public:
    enum FilterType
    {
        LPF   = 0,  // Low-pass: series L, shunt C
        HPF   = 1,  // High-pass: series C, shunt L
        BPF   = 2,  // Band-pass: series LC resonance
        Notch = 3   // Notch: parallel LC anti-resonance
    };

    struct Params
    {
        float cutoffHz     = 1000.0f;  // Center/cutoff frequency (Hz)
        float resonance    = 0.5f;     // Q / resonance 0.0–1.0
        float drive        = 0.0f;     // Inductor core saturation drive 0.0–1.0
        float temperature  = 25.0f;    // Operating temperature (°C)
        int   filterType   = LPF;      // Filter topology
    };

    PassiveLCFilter() noexcept
        : inductor(InductorPrimitive::HaloInductor())
    {}

    explicit PassiveLCFilter(const InductorPrimitive::Spec& inductorSpec) noexcept
        : inductor(inductorSpec)
    {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = static_cast<size_t>(std::max(1, numChannels));

        // Prepare inductor
        inductor.prepare(sampleRate);

        // Per-channel state
        channelState.resize(nCh);
        for (auto& ch : channelState)
        {
            ch.s1 = 0.0;
            ch.s2 = 0.0;
            ch.capElement.prepare(sampleRate);
        }

        updateCoefficients(1000.0f, 0.5f, 25.0f);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        inductor.reset();
        for (auto& ch : channelState)
        {
            ch.s1 = 0.0;
            ch.s2 = 0.0;
        }
    }

    void setCutoffHz(float hz) noexcept
    {
        cutoffHz = std::clamp(static_cast<double>(hz), 20.0, sampleRate * 0.49);
        updateCoefficients(hz, static_cast<float>(resonance), lastTemp);
    }

    void setResonance(float r) noexcept
    {
        resonance = std::clamp(static_cast<double>(r), 0.0, 1.0);
        updateCoefficients(static_cast<float>(cutoffHz), r, lastTemp);
    }

    // =====================================================================
    //  Per-sample processing
    // =====================================================================

    inline float process(int channel, float x, int filterType = LPF,
                         float drive = 0.0f, float temperature = 25.0f) noexcept
    {
        if (channelState.empty()) return x;
        const size_t ch = static_cast<size_t>(std::clamp(channel, 0, static_cast<int>(channelState.size()) - 1));
        auto& st = channelState[ch];

        double v = static_cast<double>(x);

        // Apply drive to inductor (core saturation)
        double driveScale = 1.0 + static_cast<double>(drive) * 3.0;
        double inductorInput = v * driveScale;

        // Process through inductor (adds saturation, core loss, resonance)
        double inductorOut = inductor.process(inductorInput, temperature);
        inductorOut /= driveScale;

        // TPT/ZDF SVF (Zavalishin 2012) — unconditionally stable for any g or Q.
        // Forward-Euler form is unstable when g = tan(π·fc/SR) > 1 (fc near Nyquist);
        // the TPT form eliminates the delay-free loop and avoids that divergence.
        const double k  = qInv;                        // damping = 1/Q
        const double a1 = 1.0 / (1.0 + g * (g + k));  // normalisation
        const double a2 = g * a1;
        const double a3 = g * a2;                      // g² · a1
        const double v3 = v - st.s2;
        const double bp = a1 * st.s1 + a2 * v3;       // band-pass
        const double lp = st.s2 + a2 * st.s1 + a3 * v3; // low-pass
        const double hp = v - k * bp - lp;             // high-pass

        // Subtle inductor coloring on BP resonant path (saturation character)
        const double inductorColor = (inductorOut - v) * 0.1;

        // Update states (TPT state advance: s_new = 2*output - s_old)
        st.s1 = FastMath::sanitize(2.0 * (bp + inductorColor * 0.05) - st.s1);
        st.s2 = FastMath::sanitize(2.0 * lp - st.s2);

        double output;
        switch (filterType)
        {
            case LPF:   output = lp; break;
            case HPF:   output = hp; break;
            case BPF:   output = bp; break;
            case Notch: output = hp + lp; break;
            default:    output = lp; break;
        }

        // Insertion loss from inductor DCR
        output *= insertionLoss;

        return static_cast<float>(FastMath::sanitize(output));
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.resonance, params.temperature);
        return process(channel, x, params.filterType, params.drive, params.temperature);
    }

    // =====================================================================
    //  Block processing
    // =====================================================================

    void processBlock(float* const* io, int numChannels, int numSamples,
                      const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.resonance, params.temperature);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params.filterType, params.drive, params.temperature);
    }

    // =====================================================================
    //  Accessors
    // =====================================================================

    const InductorPrimitive& getInductor() const noexcept { return inductor; }

    // Resonant frequency of current LC combination
    double getResonantFreqHz() const noexcept { return cutoffHz; }

    // Effective Q at current settings
    double getEffectiveQ() const noexcept { return 1.0 / std::max(qInv, 0.01); }

private:
    void updateCoefficients(float fc, float r, float temperature) noexcept
    {
        cutoffHz = std::clamp(static_cast<double>(fc), 20.0, sampleRate * 0.49);
        resonance = std::clamp(static_cast<double>(r), 0.0, 1.0);
        lastTemp = temperature;

        // Temperature-dependent tuning via inductor permeability
        double tempScale = inductor.muScale(temperature);
        double effectiveFreq = cutoffHz * tempScale;

        // SVF coefficient
        g = std::tan(kPi * effectiveFreq / sampleRate);

        // Q from resonance parameter + inductor Q contribution
        double inductorQ = inductor.qAtFreq(effectiveFreq);
        double baseQ = 0.5 + resonance * 15.0;  // 0.5 to 15.5
        double effectiveQ = std::min(baseQ, inductorQ);  // Limited by inductor losses
        qInv = 1.0 / std::max(effectiveQ, 0.5);

        // Insertion loss from inductor DCR
        const auto& inductorSpec = inductor.getSpec();
        double xl = 2.0 * kPi * effectiveFreq * inductorSpec.inductanceH;
        double totalR = inductorSpec.dcResistance;
        insertionLoss = xl / (xl + totalR + 0.01);
        insertionLoss = std::clamp(insertionLoss, 0.5, 1.0);
    }

    static constexpr double kPi = 3.14159265358979323846;

    // === Component layer (Parts) ===
    InductorPrimitive inductor;

    struct ChannelState
    {
        double s1 = 0.0;
        double s2 = 0.0;
        RC_Element capElement;
    };

    double sampleRate = 44100.0;
    double cutoffHz = 1000.0;
    double resonance = 0.5;
    float lastTemp = 25.0f;
    double g = 0.0;
    double qInv = 1.0;
    double insertionLoss = 0.95;

    std::vector<ChannelState> channelState;
};
