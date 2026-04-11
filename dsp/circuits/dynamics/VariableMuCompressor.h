#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/TubeTriode.h"

// variable-mu tube compressor emulation
// - Uses remote-cutoff tube (6386 dual triode) as gain element
// - Push-pull balanced circuit → even harmonic cancellation
// - 6 time constants (TC 1–6 selector)
// - Slight saturation from output stage tube
//
// 4-layer architecture:
//   Parts: TubeTriode (6386 VariableMu + 12BH7 output)
//   → Circuit: VariableMuCompressor (SC + push-pull balance + time constants)
class VariableMuCompressor
{
public:
    // time constants (TC selector: 1–6)
    struct TimeConstant
    {
        double attackMs;
        double releaseMs;
    };

    static constexpr TimeConstant kTimeConstants[6] = {
        { 0.2,   300.0 },   // TC 1: fastest
        { 0.2,   800.0 },   // TC 2
        { 0.4,  2000.0 },   // TC 3
        { 0.4,  5000.0 },   // TC 4
        { 0.8,  2000.0 },   // TC 5
        { 0.8,  5000.0 },   // TC 6: slowest
    };

    struct Params
    {
        float inputGain  = 0.5f;    // Input gain 0.0–1.0
        float threshold  = 0.5f;    // compression threshold 0.0–1.0
        float outputGain = 0.5f;    // Makeup gain 0.0–1.0
        int   timeConstant = 0;     // TC selector 0–5
        float mix        = 1.0f;    // Dry/Wet
    };

    VariableMuCompressor() noexcept
        : gainTube(TubeTriode::T12AX7()),
          outputTube(TubeTriode::T12BH7()),
          vmuSpec(TubeTriode::Tube6386())
    {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        chState.resize(nCh);
        for (auto& st : chState) st = ChannelState{};
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& st : chState) st = ChannelState{};
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);
        auto& st = chState[ch];

        // === Input gain ===
        const double inputGain = 1.0 + (double)params.inputGain * (double)params.inputGain * 15.0;
        double v = (double)x * inputGain;
        const double dry = v;

        // === Sidechain ===
        const double absV = std::abs(v);

        // get attack/release from TC selector
        const auto& tc = kTimeConstants[std::clamp(params.timeConstant, 0, 5)];
        const double attAlpha = msToAlpha(tc.attackMs);
        const double relAlpha = msToAlpha(tc.releaseMs);

        // envelope detection (peak, asymmetric)
        if (absV > st.envelope)
            st.envelope += attAlpha * (absV - st.envelope);
        else
            st.envelope += relAlpha * (absV - st.envelope);

        // === Variable-mu gain calculation ===
        // threshold: params.threshold 0→-40dB, 0.5→-20dB, 1.0→0dB
        const double threshDb = -40.0 + (double)params.threshold * 40.0;

        // Calculate overshoot in dB domain (same as FET compressor)
        const double envDb = (st.envelope > 1e-10)
            ? 20.0 * std::log10(st.envelope) : -200.0;

        // G2 control voltage: envelope amount exceeding threshold (dB scaling)
        double controlVoltage = 0.0;
        if (envDb > threshDb)
        {
            // soft knee: compression starts smoothly near threshold (reproducing real hardware)
            constexpr double kSoftKneeDb = 6.0;
            const double overDb = envDb - threshDb;
            double effectiveOverDb;
            if (overDb < kSoftKneeDb)
            {
                // Soft-knee region: gradual compression via 2nd-order curve
                effectiveOverDb = overDb * overDb / (2.0 * kSoftKneeDb);
            }
            else
            {
                effectiveOverDb = overDb - kSoftKneeDb * 0.5;
            }
            // 20dBover cv=1.0 → gain=exp(-3)≈0.05 (-26dB GR)
            controlVoltage = effectiveOverDb / 20.0;
        }

        // variable-mu characteristic (delegated to TubeTriode)
        double gain = TubeTriode::variableMuGain(controlVoltage, vmuSpec);
        gain = std::clamp(gain, kMinGain, 1.0);

        // Gain smoothing (variable-mu tube slew-rate limiting)
        st.smoothedGain += (gain - st.smoothedGain) * kGainSlewRate;

        // === Push-pull balanced processing (delegated to TubeTriode) ===
        v *= st.smoothedGain;
        v = gainTube.pushPullBalance(v);

        // === Output stage tube saturation (delegated to TubeTriode) ===
        v = outputTube.outputSaturate(v);

        // === Makeup gain ===
        const double makeupGain = (double)params.outputGain * (double)params.outputGain * 4.0;
        v *= makeupGain;

        // === Dry/Wet ===
        const double mix = std::clamp((double)params.mix, 0.0, 1.0);
        v = dry * (1.0 - mix) * makeupGain + v * mix;

        st.lastGainReduction = st.smoothedGain;
        return (float)v;
    }

    /** Process with external sidechain signal for envelope detection.
        Audio path uses x, detection uses scSignal. */
    inline float processWithSidechain(int channel, float x, float scSignal,
                                       const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);
        auto& st = chState[ch];

        const double inputGain = 1.0 + (double)params.inputGain * (double)params.inputGain * 15.0;
        double v = (double)x * inputGain;
        const double dry = v;

        // Sidechain detection uses the external signal (e.g. HPF'd)
        const double absV = std::abs((double)scSignal * inputGain);

        const auto& tc = kTimeConstants[std::clamp(params.timeConstant, 0, 5)];
        const double attAlpha = msToAlpha(tc.attackMs);
        const double relAlpha = msToAlpha(tc.releaseMs);

        if (absV > st.envelope)
            st.envelope += attAlpha * (absV - st.envelope);
        else
            st.envelope += relAlpha * (absV - st.envelope);

        const double threshDb = -40.0 + (double)params.threshold * 40.0;
        const double envDb = (st.envelope > 1e-10)
            ? 20.0 * std::log10(st.envelope) : -200.0;

        double controlVoltage = 0.0;
        if (envDb > threshDb)
        {
            constexpr double kSoftKneeDb = 6.0;
            const double overDb = envDb - threshDb;
            double effectiveOverDb;
            if (overDb < kSoftKneeDb)
                effectiveOverDb = overDb * overDb / (2.0 * kSoftKneeDb);
            else
                effectiveOverDb = overDb - kSoftKneeDb * 0.5;
            controlVoltage = effectiveOverDb / 20.0;
        }

        double gain = TubeTriode::variableMuGain(controlVoltage, vmuSpec);
        gain = std::clamp(gain, kMinGain, 1.0);
        st.smoothedGain += (gain - st.smoothedGain) * kGainSlewRate;

        v *= st.smoothedGain;
        v = gainTube.pushPullBalance(v);
        v = outputTube.outputSaturate(v);

        const double makeupGain = (double)params.outputGain * (double)params.outputGain * 4.0;
        v *= makeupGain;

        const double mix = std::clamp((double)params.mix, 0.0, 1.0);
        v = dry * (1.0 - mix) * makeupGain + v * mix;

        st.lastGainReduction = st.smoothedGain;
        return (float)v;
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

    float getGainReductionDb(int channel) const noexcept
    {
        if (channel < 0 || (size_t)channel >= chState.size()) return 0.0f;
        double gr = chState[(size_t)channel].lastGainReduction;
        if (gr <= 0.001) return -60.0f;
        return (float)(20.0 * std::log10(gr));
    }

private:
    // === Variable-mu circuit constants ===
    static constexpr double kMinGain       = 0.001;
    static constexpr double kGainSlewRate  = 0.05;

    // === Component layer (Parts) ===
    TubeTriode gainTube;    // 6386 variable-mu tube
    TubeTriode outputTube;  // 12BH7 output stage
    TubeTriode::VariableMuSpec vmuSpec;  // 6386 variable-mu specification

    struct ChannelState
    {
        double envelope          = 0.0;
        double smoothedGain      = 1.0;
        double lastGainReduction = 1.0;
    };

    inline double msToAlpha(double ms) const noexcept
    {
        if (ms <= 0.0) return 1.0;
        return 1.0 - std::exp(-1.0 / (sampleRate * ms * 0.001));
    }

    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<ChannelState> chState;
};
