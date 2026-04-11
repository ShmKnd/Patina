#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/VcaPrimitive.h"
#include "../../parts/RC_Element.h"
#include "../../parts/DiodePrimitive.h"

// VCA compressor emulation (classic large-format console / classic dynamics processor 160 style)
// - THAT 2180 Blackmer transparent gain control via VCA cell
// - Feedforward topology
// - RMS envelope via diode rectifier + RC detection circuit
// - Selectable soft knee / hard knee
// - Precise ratio control (1.5:1 to ∞:1)
// - auto makeup gain support
//
// 4-layer architecture:
//   Parts: VcaPrimitive (THAT2180) + RC_Element (detection RC) + DiodePrimitive (rectifier)
//   → Circuit: VcaCompressor (sidechain detection + VCA gain cell)
//
// Actual circuit topology:
//   Audio ─────────────────────────────── VCA ──→ Output
//                                          ↑ gain
//   Audio → DiodeBridge → ┬─ R_att (attack diode ON) ─┬─ C → √ → dB → GR calculation
//           (rectifier)         └─ R_rel (attack diode OFF) ─┘ (detection RC)
//
// use: master bus, mix bus, transparent dynamics control
class VcaCompressor
{
public:
    struct Params
    {
        float threshold  = 0.5f;    // 0.0–1.0 → -40dB ~ 0dBFS
        float ratio      = 0.3f;    // 0.0–1.0 → 1.5:1 ~ ∞:1
        float attack     = 0.3f;    // 0.0–1.0 → 0.1ms ~ 80ms
        float release    = 0.5f;    // 0.0–1.0 → 50ms ~ 1200ms
        float outputGain = 0.5f;    // Makeup gain 0.0–1.0
        float mix        = 1.0f;    // Dry/Wet
        int   kneeMode   = 0;       // 0=soft knee, 1=hard knee
    };

    VcaCompressor() noexcept
        : vca(VcaPrimitive::THAT2180(), 500)
        , scDiode(DiodePrimitive::Si1N4148())
    {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        chState.resize(nCh);
        for (auto& st : chState)
        {
            st = ChannelState{};
            st.detectorRC = RC_Element(kDefaultDetectorR, kDetectorCap);
            st.detectorRC.prepare(sampleRate);
        }
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& st : chState)
        {
            st = ChannelState{};
            st.detectorRC = RC_Element(kDefaultDetectorR, kDetectorCap);
            st.detectorRC.prepare(sampleRate);
        }
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);
        auto& st = chState[ch];

        const double dry = (double)x;

        // === Sidechain: diode bridge rectification ===
        // Precision full-wave rectifier — due to Si diode (1N4148) Vf and nonlinear I-V
        // low-level signal detection sensitivity exhibits characteristics unique to analog detection circuits
        const double rectified = std::abs(scDiode.saturate((double)x));

        // === RMS detection (root mean square) ===
        const double xSq = rectified * rectified;

        // === RC detection circuit — switches attack/release resistance via diode switching ===
        // Actual circuit: when rectified signal exceeds detection capacitor voltage, the attack diode
        // conducts and rapidly charges the capacitor through low-resistance path (R_att)。
        // when signal drops, diode blocks and slowly discharges through high-resistance path (R_rel)。
        const double attackR  = paramToAttackR(params.attack);
        const double releaseR = paramToReleaseR(params.release);

        if (xSq > st.prevEnvSq)
            st.detectorRC.setRC(attackR, kDetectorCap);
        else
            st.detectorRC.setRC(releaseR, kDetectorCap);

        const double envSq = st.detectorRC.processLPF(xSq);
        st.prevEnvSq = envSq;

        const double rmsLevel = std::sqrt(std::max(envSq, 1e-20));

        // === Threshold and ratio ===
        const double threshDb = -40.0 + (double)params.threshold * 40.0;
        const double ratioVal = 1.5 + (double)params.ratio * (double)params.ratio * 48.5; // 1.5:1 ~ 50:1

        const double envDb = 20.0 * std::log10(std::max(rmsLevel, 1e-10));

        // === Gain reduction calculation ===
        double gainDb = 0.0;
        if (envDb > threshDb)
        {
            const double overDb = envDb - threshDb;

            if (params.kneeMode == 0)
            {
                // soft knee (6dB width)
                constexpr double kKneeDb = 6.0;
                if (overDb < kKneeDb)
                {
                    const double halfOver = overDb / kKneeDb;
                    gainDb = -(overDb * halfOver * (1.0 - 1.0 / ratioVal)) * 0.5;
                }
                else
                {
                    gainDb = -(overDb * (1.0 - 1.0 / ratioVal));
                }
            }
            else
            {
                // hard knee
                gainDb = -(overDb * (1.0 - 1.0 / ratioVal));
            }
        }

        double gain = std::pow(10.0, gainDb / 20.0);
        gain = std::clamp(gain, kMinGain, 1.0);

        // Gain smoothing（Blackmer cell bandwidth limitation）
        const double smoothAlpha = 0.01;
        st.smoothedGain += smoothAlpha * (gain - st.smoothedGain);

        // === VCA gain application (VcaPrimitive — THAT2180 log/antilog BJT pair) ===
        double v = vca.applyGain(dry, st.smoothedGain);

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

        const double dry = (double)x;

        // Sidechain detection uses external signal (e.g. HPF'd)
        const double rectified = std::abs(scDiode.saturate((double)scSignal));
        const double xSq = rectified * rectified;

        const double attackR  = paramToAttackR(params.attack);
        const double releaseR = paramToReleaseR(params.release);

        if (xSq > st.prevEnvSq)
            st.detectorRC.setRC(attackR, kDetectorCap);
        else
            st.detectorRC.setRC(releaseR, kDetectorCap);

        const double envSq = st.detectorRC.processLPF(xSq);
        st.prevEnvSq = envSq;

        const double rmsLevel = std::sqrt(std::max(envSq, 1e-20));

        const double threshDb = -40.0 + (double)params.threshold * 40.0;
        const double ratioVal = 1.5 + (double)params.ratio * (double)params.ratio * 48.5;
        const double envDb = 20.0 * std::log10(std::max(rmsLevel, 1e-10));

        double gainDb = 0.0;
        if (envDb > threshDb)
        {
            const double overDb = envDb - threshDb;
            if (params.kneeMode == 0)
            {
                constexpr double kKneeDb = 6.0;
                if (overDb < kKneeDb)
                {
                    const double halfOver = overDb / kKneeDb;
                    gainDb = -(overDb * halfOver * (1.0 - 1.0 / ratioVal)) * 0.5;
                }
                else
                    gainDb = -(overDb * (1.0 - 1.0 / ratioVal));
            }
            else
                gainDb = -(overDb * (1.0 - 1.0 / ratioVal));
        }

        double gain = std::pow(10.0, gainDb / 20.0);
        gain = std::clamp(gain, kMinGain, 1.0);

        const double smoothAlpha = 0.01;
        st.smoothedGain += smoothAlpha * (gain - st.smoothedGain);

        double v = vca.applyGain(dry, st.smoothedGain);

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

    float getGainReductionDb(int channel = 0) const noexcept
    {
        if (channel < 0 || (size_t)channel >= chState.size()) return 0.0f;
        double gr = chState[(size_t)channel].lastGainReduction;
        if (gr <= 0.001) return -60.0f;
        return (float)(20.0 * std::log10(gr));
    }

private:
    // === VCA compressor circuit constants ===
    // Detection RC circuit: C = 1μF (electrolytic capacitor — standard for analog detection circuits)
    static constexpr double kDetectorCap     = 1.0e-6;   // 1μF
    static constexpr double kDefaultDetectorR = 300.0;    // default R at idle
    // Attack resistance: R_att = 100Ω (RC=0.1ms) ~ 80kΩ (RC=80ms)  @ C=1μF
    static constexpr double kAttackRMin      = 100.0;     // 100Ω
    static constexpr double kAttackRMax      = 80000.0;   // 80kΩ
    // Release resistance: R_rel = 50kΩ (RC=50ms) ~ 1.2MΩ (RC=1200ms) @ C=1μF
    static constexpr double kReleaseRMin     = 50000.0;   // 50kΩ
    static constexpr double kReleaseRMax     = 1200000.0; // 1.2MΩ
    static constexpr double kMinGain         = 0.001;

    // === Component layer (Parts) ===
    VcaPrimitive   vca;       // THAT 2180 Blackmer VCA gain cell
    DiodePrimitive scDiode;   // sidechain rectifier diode (Si 1N4148)

    struct ChannelState
    {
        RC_Element detectorRC;              // detection RC circuit (per-channel capacitor state)
        double     prevEnvSq         = 0.0; // previous sample's envelope² (for attack/release switching)
        double     smoothedGain      = 1.0;
        double     lastGainReduction = 1.0;
    };

    // User parameters → physical resistance value mapping (quadratic curve)
    static inline double paramToAttackR(float attack) noexcept
    {
        double t = (double)attack * (double)attack;
        return kAttackRMin + t * (kAttackRMax - kAttackRMin);
    }

    static inline double paramToReleaseR(float release) noexcept
    {
        double t = (double)release * (double)release;
        return kReleaseRMin + t * (kReleaseRMax - kReleaseRMin);
    }

    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<ChannelState> chState;
};
