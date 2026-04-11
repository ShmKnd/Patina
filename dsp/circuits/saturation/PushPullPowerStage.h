#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../core/FastMath.h"
#include "../../parts/PowerPentode.h"
#include "../../parts/TransformerPrimitive.h"

// =============================================================================
//  PushPullPowerStage — Class AB Push-Pull Tube Power Amplifier
//
//  Physical model of a tube power amplifier output stage with matched pentode pair
//  and output transformer. Models the key behaviors that define the sound of
//  classic guitar/bass amplifiers and hi-fi tube power amps.
//
//  Signal flow:
//    Input → Phase Inverter (split to +/-) →
//    PowerPentode A (positive half) + PowerPentode B (negative half, inverted) →
//    Differential summation → Output Transformer → Speaker output
//
//  Physical characteristics:
//    - Class AB biasing: adjustable from cold (more crossover) to hot (class A)
//    - Phase inverter imbalance: asymmetric drive to push/pull tubes
//    - Crossover distortion: dead zone near zero crossing (class AB notch)
//    - Tube mismatch: slight asymmetry between matched pair
//    - Output transformer saturation: LF sag, HF rolloff, core hysteresis
//    - Negative feedback: adjustable global NFB from output to input
//    - Power supply sag: B+ voltage drop under heavy load (compression)
//    - Screen grid sag: shared screen supply → tube interaction
//    - Presence control: negative feedback frequency shaping
//
//  Topology presets:
//    Marshall50W:   EL34 × 2, British console OT, moderate NFB (JTM45/Plexi)
//    FenderTwin:    6L6GC × 2, American OT, heavy NFB (clean headroom)
//    VoxAC30:       EL84 × 2, hot bias (near class A), low NFB
//    HiFi_KT88:     KT88 × 2, ultralinear OT, high NFB (audiophile)
//    FenderDeluxe:  6V6GT × 2, compact OT, moderate NFB (sweet breakup)
//
//  4-layer architecture:
//    Parts: PowerPentode × 2 + TransformerPrimitive × 1
//    → Circuit: PushPullPowerStage (class AB power amplifier)
//
//  Usage:
//    PushPullPowerStage amp;
//    amp.prepare(2, 48000.0);
//    PushPullPowerStage::Params p;
//    p.inputGainDb = 0.0f;
//    p.bias = 0.65f;
//    float out = amp.process(0, input, p);
// =============================================================================
class PushPullPowerStage
{
public:
    struct Params
    {
        float inputGainDb      = 0.0f;    // Input gain to power stage (dB)
        float bias             = 0.65f;   // Bias point 0.0 (cold/class B) – 1.0 (hot/class A)
        float piImbalance      = 0.02f;   // Phase inverter imbalance 0.0 (perfect) – 0.2 (sloppy)
        float tubeMismatch     = 0.03f;   // Tube pair mismatch 0.0 (matched) – 0.2 (worn)
        float negativeFeedback = 0.3f;    // Global negative feedback 0.0 (none) – 1.0 (heavy)
        float presenceHz       = 5000.0f; // Presence control frequency (Hz) — NFB shelf corner
        float sagAmount        = 0.5f;    // Power supply sag 0.0 (regulated) – 1.0 (vintage rectifier)
        float outputLevel      = 0.7f;    // Output level 0.0 – 1.0
        float temperature      = 35.0f;   // Operating temperature (°C)
    };

    // =====================================================================
    //  Topology presets
    // =====================================================================

    // Marshall JTM45 / Plexi — EL34 push-pull, British output transformer
    // Aggressive midrange, pronounced crossover distortion when cranked
    static PushPullPowerStage Marshall50W() noexcept
    {
        return PushPullPowerStage(
            PowerPentode::EL34(), PowerPentode::EL34(),
            TransformerPrimitive::BritishConsole());
    }

    // Fender Twin Reverb — 6L6GC push-pull, American output transformer
    // Clean headroom, tight bass, heavy negative feedback
    static PushPullPowerStage FenderTwin() noexcept
    {
        return PushPullPowerStage(
            PowerPentode::_6L6GC(), PowerPentode::_6L6GC(),
            TransformerPrimitive::AmericanConsole());
    }

    // Vox AC30 — EL84 push-pull, hot bias (near class A)
    // Chimey breakup, rich harmonics, minimal negative feedback
    static PushPullPowerStage VoxAC30() noexcept
    {
        return PushPullPowerStage(
            PowerPentode::EL84(), PowerPentode::EL84(),
            TransformerPrimitive::BritishConsole());
    }

    // Hi-Fi KT88 ultralinear — KT88 push-pull, high-quality output transformer
    // Massive headroom, low THD, deep bass extension
    static PushPullPowerStage HiFi_KT88() noexcept
    {
        // Use API output transformer for hi-fi quality
        return PushPullPowerStage(
            PowerPentode::KT88(), PowerPentode::KT88(),
            TransformerPrimitive::API2520Output());
    }

    // Fender Deluxe — 6V6GT push-pull, compact output transformer
    // Sweet, singing breakup, low headroom, classic blues tone
    static PushPullPowerStage FenderDeluxe() noexcept
    {
        return PushPullPowerStage(
            PowerPentode::_6V6GT(), PowerPentode::_6V6GT(),
            TransformerPrimitive::CompactFetOutput());
    }

    // =====================================================================
    //  Constructors
    // =====================================================================

    PushPullPowerStage() noexcept
        : tubeA(PowerPentode::EL34()),
          tubeB(PowerPentode::EL34()),
          outputXfmr(TransformerPrimitive::BritishConsole()),
          rng(42), normalDist(0.0, 1.0)
    {}

    PushPullPowerStage(const PowerPentode::Spec& tubeSpecA,
                       const PowerPentode::Spec& tubeSpecB,
                       const TransformerPrimitive::Spec& xfmrSpec) noexcept
        : tubeA(tubeSpecA),
          tubeB(tubeSpecB),
          outputXfmr(xfmrSpec),
          rng(42), normalDist(0.0, 1.0)
    {}

    // =====================================================================
    //  Lifecycle
    // =====================================================================

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = static_cast<size_t>(std::max(1, numChannels));

        // Prepare L2 parts
        tubeA.prepare(sampleRate);
        tubeB.prepare(sampleRate);
        outputXfmr.prepare(sampleRate);

        // Per-channel state
        channelState.resize(nCh);
        for (auto& ch : channelState)
        {
            ch.dcBlockX = 0.0;
            ch.dcBlockY = 0.0;
            ch.nfbState = 0.0;
            ch.sagState = 0.0;
            ch.presenceState = 0.0;
        }

        // DC block coefficient (~10 Hz HP)
        dcBlockAlpha = 1.0 / (1.0 + 2.0 * kPi * 10.0 / sampleRate);

        // Power supply sag time constant (~10 Hz response)
        sagAlpha = 1.0 - std::exp(-2.0 * kPi * 10.0 / sampleRate);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        tubeA.reset();
        tubeB.reset();
        outputXfmr.reset();
        for (auto& ch : channelState)
        {
            ch.dcBlockX = 0.0;
            ch.dcBlockY = 0.0;
            ch.nfbState = 0.0;
            ch.sagState = 0.0;
            ch.presenceState = 0.0;
        }
    }

    // =====================================================================
    //  Per-sample processing
    // =====================================================================

    inline float process(int channel, float x, const Params& params) noexcept
    {
        if (channelState.empty()) return x;
        const size_t ch = static_cast<size_t>(
            std::clamp(channel, 0, static_cast<int>(channelState.size()) - 1));
        auto& st = channelState[ch];

        // Input gain
        double inputGain = std::pow(10.0, static_cast<double>(params.inputGainDb) / 20.0);
        double v = static_cast<double>(x) * inputGain;

        // =================================================================
        // 1. Global negative feedback (frequency-shaped)
        // =================================================================
        double nfbAmount = static_cast<double>(params.negativeFeedback);
        if (nfbAmount > 0.001)
        {
            // Presence control: HPF on feedback path
            // Below presenceHz: full feedback → clean. Above: reduced feedback → bright
            double presenceAlpha = 1.0 - std::exp(-2.0 * kPi
                * static_cast<double>(params.presenceHz) / sampleRate);
            st.presenceState += presenceAlpha * (st.nfbState - st.presenceState);
            double shapedFb = st.presenceState;  // LP filtered feedback

            v -= shapedFb * nfbAmount * 0.5;
        }

        // =================================================================
        // 2. Phase inverter (cathodyne or long-tail pair)
        // =================================================================
        double imbalance = static_cast<double>(params.piImbalance);
        double driveA =  v * (1.0 + imbalance * 0.5);   // Push (non-inverted)
        double driveB = -v * (1.0 - imbalance * 0.5);   // Pull (inverted)

        // =================================================================
        // 3. Power supply sag (shared B+ rail)
        // =================================================================
        double sagTarget = (std::abs(driveA) + std::abs(driveB)) * 0.5;
        st.sagState += sagAlpha * (sagTarget - st.sagState);
        double sagScale = 1.0 - st.sagState * static_cast<double>(params.sagAmount) * 0.3;
        sagScale = std::clamp(sagScale, 0.6, 1.0);

        driveA *= sagScale;
        driveB *= sagScale;

        // =================================================================
        // 4. Tube mismatch (slight asymmetry in matched pair)
        // =================================================================
        double mismatch = static_cast<double>(params.tubeMismatch);
        driveA *= (1.0 + mismatch * 0.5);
        driveB *= (1.0 - mismatch * 0.5);

        // =================================================================
        // 5. Bias point — mapped to PowerPentode bias parameter
        //    0.0 (cold/class B) → bias = -0.5
        //    0.65 (normal class AB) → bias = -0.3
        //    1.0 (hot/class A) → bias = -0.1
        // =================================================================
        double biasPoint = -(0.5 - static_cast<double>(params.bias) * 0.4);

        // =================================================================
        // 6. Push-pull pentode pair processing
        // =================================================================
        double plateA = tubeA.process(driveA, biasPoint);
        double plateB = tubeB.process(driveB, biasPoint);

        // Differential summation (output transformer primary)
        // In push-pull, even harmonics cancel, odd harmonics add
        double differential = plateA - plateB;

        // =================================================================
        // 7. Output transformer
        // =================================================================
        double xfmrOut = outputXfmr.process(differential * 0.5,
                                            params.temperature);

        // Store for negative feedback loop
        st.nfbState = xfmrOut;

        // =================================================================
        // 8. Output level & DC block
        // =================================================================
        double v_out = xfmrOut * static_cast<double>(params.outputLevel);

        // DC block (HP filter)
        double dcOut = dcBlockAlpha * (st.dcBlockY + v_out - st.dcBlockX);
        st.dcBlockX = v_out;
        st.dcBlockY = dcOut;

        return static_cast<float>(FastMath::sanitize(dcOut));
    }

    // =====================================================================
    //  Block processing
    // =====================================================================

    void processBlock(float* const* io, int numChannels, int numSamples,
                      const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

    // =====================================================================
    //  Accessors
    // =====================================================================

    const PowerPentode& getTubeA() const noexcept { return tubeA; }
    const PowerPentode& getTubeB() const noexcept { return tubeB; }
    const TransformerPrimitive& getOutputTransformer() const noexcept { return outputXfmr; }

    // Thermal state of both tubes (average)
    double getAverageThermalState() const noexcept
    {
        return (tubeA.getThermalState() + tubeB.getThermalState()) * 0.5;
    }

private:
    static constexpr double kPi = 3.14159265358979323846;

    // === Component layer (Parts) ===
    PowerPentode tubeA;             // Push tube (positive half)
    PowerPentode tubeB;             // Pull tube (negative half)
    TransformerPrimitive outputXfmr; // Output transformer

    struct ChannelState
    {
        double dcBlockX      = 0.0;
        double dcBlockY      = 0.0;
        double nfbState      = 0.0;  // Negative feedback state (previous output)
        double sagState      = 0.0;  // Power supply sag state
        double presenceState = 0.0;  // Presence control LP state
    };

    double sampleRate    = 44100.0;
    double dcBlockAlpha  = 0.999;
    double sagAlpha      = 0.01;

    std::vector<ChannelState> channelState;

    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;
};
