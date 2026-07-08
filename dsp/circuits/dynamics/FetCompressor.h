#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/JFET_Primitive.h"

// classic FET compressor/limiter-style compressor emulation
// - Ultra-fast attack via JFET (2N5457 type) VCA
// - Coloration from Class A output transformer
// - 4 ratios + "All-Buttons In" (nuke) mode
// - Program-dependent release
//
// 4-layer architecture:
//   Parts: JFET_Primitive (2N5457)
//   → Circuit: FetCompressor (SC + VCA + output transformer)
class FetCompressor
{
public:
    enum class Ratio : int
    {
        R4to1  = 0,   // 4:1
        R8to1  = 1,   // 8:1
        R12to1 = 2,   // 12:1
        R20to1 = 3,   // 20:1
        All    = 4    // All-Buttons (nuke)
    };

    struct Params
    {
        float inputGain  = 0.5f;    // Input gain 0.0–1.0 → 0dB–+40dB
        float outputGain = 0.5f;    // Makeup gain 0.0–1.0
        float attack     = 0.5f;    // attack 0.0–1.0 → 20μs–800μs
        float release    = 0.5f;    // release 0.0–1.0 → 50ms–1100ms
        int   ratio      = 0;       // 0=4:1, 1=8:1, 2=12:1, 3=20:1, 4=All
        float mix        = 1.0f;    // Dry/Wet
    };

    FetCompressor() noexcept
        : jfet(JFET_Primitive::N2N5457(), 701)
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
        // xfmrCoreSatState は ChannelState に移動済み — ChannelState{} で自動リセット
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);
        auto& st = chState[ch];

        // === Input gain → threshold mapping ===
        // inputGain controls how far the signal is "pushed into" the fixed threshold.
        // Instead of amplifying the signal and dividing back (which causes gain-modulation
        // artifacts on low-frequency signals), we lower the effective threshold.
        //
        // 旧実装の問題点:
        //   v = x * inputGain → 検波 → gain適用 → v /= inputGain
        //   この方式では gain(t) の時間変化が inputGain 倍に増幅された信号に掛かり、
        //   低域信号の各周期でゲインが微小に変わるたび波形に不連続な折れ曲がりが生じる。
        //   /inputGain で戻しても非線形なゲイン変調痕は除去できない。
        //
        // 新実装: 元信号(dry)で検波し、inputGainはスレッショルドのオフセットとして反映。
        // VcaCompressorと同様「信号を増幅せず、閾値を下げる」方式。
        const double inputGainDb = 20.0 * std::log10(
            1.0 + (double)params.inputGain * (double)params.inputGain * 39.0);
        const double dry = (double)x;

        // === Sidechain: 元信号ベースの RMS 検波 ===
        // 検波は元信号 (dry) に対して行う。inputGain増幅後の信号を使わないことで、
        // sens ノブによる検波感度の過敏反応を防ぐ。
        const double absV     = std::abs(dry);
        const double rmsAlpha = 1.0 - std::exp(-1.0 / (sampleRate * 0.010));  // τ = 10ms
        st.rmsState += rmsAlpha * (absV * absV - st.rmsState);
        const double scLevel  = std::sqrt(std::max(st.rmsState, 1e-20));

        // attack/release time constants (quadratic scale)
        const double attMs  = kAttackMinMs
                            + (double)params.attack * (double)params.attack
                              * (kAttackMaxMs - kAttackMinMs);
        const double relMs  = kReleaseMinMs
                            + (double)params.release * (double)params.release
                              * (kReleaseMaxMs - kReleaseMinMs);

        const double attAlpha = msToAlpha(attMs);
        const double relAlpha = msToAlpha(relMs);

        if (scLevel > st.envelope)
            st.envelope += attAlpha * (scLevel - st.envelope);
        else
            st.envelope += relAlpha * (scLevel - st.envelope);

        // === Gain calculation ===
        // Effective threshold = fixed threshold − inputGainDb
        // inputGain が大きいほどスレッショルドが下がり、より深い圧縮になる。
        const double envDb = (st.envelope > 1e-10)
            ? 20.0 * std::log10(st.envelope) : -200.0;
        const double effectiveThreshDb = kThresholdDb - inputGainDb;

        double gainDb = 0.0;
        if (envDb > effectiveThreshDb)
        {
            double overDb = envDb - effectiveThreshDb;
            double ratio = getRatio(params.ratio);

            if (params.ratio == (int)Ratio::All)
            {
                double allGr = overDb * (1.0 - 1.0 / ratio);
                if (overDb < 6.0)
                    allGr *= 0.5 + overDb / 12.0;
                gainDb = -allGr;
            }
            else
            {
                const double knee = kSoftKneeDb;
                if (overDb < knee)
                {
                    double halfOver = overDb / knee;
                    gainDb = -(overDb * halfOver * (1.0 - 1.0 / ratio)) * 0.5;
                }
                else
                {
                    gainDb = -(overDb * (1.0 - 1.0 / ratio));
                }
            }
        }

        double gain = std::pow(10.0, gainDb / 20.0);
        gain = std::clamp(gain, kMinGain, 1.0);

        // Gain smoothing — τ = 5ms (sample-rate dependent)
        const double kGainSmoothAlpha = 1.0 - std::exp(-1.0 / (sampleRate * 0.005));
        st.smoothedGain += kGainSmoothAlpha * (gain - st.smoothedGain);
        gain = st.smoothedGain;

        // === JFET VCA: ゲイン適用 + 非線形性 ===
        // 元信号に直接 gain を適用（inputGain増幅→除算サイクル廃止）
        double v = dry * gain;
        v = jfet.vcaNonlinearity(v, gain);

        // === Output transformer coloration (light core saturation, per-channel) ===
        {
            static constexpr double kSatLevel = 2.0;
            st.xfmrCoreSatState += (v - st.xfmrCoreSatState) * 0.001;
            if (std::abs(st.xfmrCoreSatState) > kSatLevel)
            {
                double amt = (std::abs(st.xfmrCoreSatState) - kSatLevel) * 0.2;
                v -= (st.xfmrCoreSatState > 0.0 ? amt : -amt);
            }
        }

        // === Makeup gain ===
        const double makeupGain = (double)params.outputGain * (double)params.outputGain * 4.0;
        v *= makeupGain;

        // === Dry/Wet ===
        const double mix = std::clamp((double)params.mix, 0.0, 1.0);
        v = dry * (1.0 - mix) * makeupGain + v * mix;

        st.lastGainReduction = gain;
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
    // === classic FET compressor/limiter circuit constants ===
    static constexpr double kThresholdDb  = -24.0;
    static constexpr double kSoftKneeDb   = 6.0;
    // Attack range: 10ms–60ms — EBS MultiComp 準拠
    // ベース楽器向け: 80Hz 半周期 6.25ms より十分長いためAM変調ノイズが発生しない。
    // Comp ノブでレシオと同時に制御: comp高い→アタック速い(10ms)、comp低い→遅い(60ms)
    static constexpr double kAttackMinMs  = 10.0;
    static constexpr double kAttackMaxMs  = 60.0;
    static constexpr double kReleaseMinMs = 100.0;
    static constexpr double kReleaseMaxMs = 1100.0;
    static constexpr double kMinGain      = 0.001;

    // === Component layer (Parts) ===
    JFET_Primitive jfet;             // 2N5457
    // xfmrCoreSatState は ChannelState に移動済み（ステレオ独立サチュレーションのため）

    // ratio table
    static constexpr double getRatio(int ratioIndex) noexcept
    {
        switch (ratioIndex)
        {
            case 1:  return 8.0;
            case 2:  return 12.0;
            case 3:  return 20.0;
            case 4:  return 50.0;  // All-Buttons
            default: return 4.0;
        }
    }

    struct ChannelState
    {
        double envelope          = 0.0;
        double rmsState          = 0.0;   // 短時間RMS（サイドチェーン平滑化用）— LF信号の半周期追跡によるゲイン変調を防ぐ
        double xfmrCoreSatState  = 0.0;   // 出力トランス磁気飽和状態（per-channel）— ステレオ独立モデリング
        double lastGainReduction = 1.0;
        double smoothedGain      = 1.0;   // prevents click on attack/release transients
    };

    inline double msToAlpha(double ms) const noexcept
    {
        if (ms <= 0.0) return 1.0;
        return 1.0 - std::exp(-1.0 / (sampleRate * ms * 0.001));
    }

    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<ChannelState> chState;
};
