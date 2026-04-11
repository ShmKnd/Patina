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
        xfmrCoreSatState = 0.0;
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);
        auto& st = chState[ch];

        // === Input gain（FET compressor controls compression amount via input gain）===
        const double inputGain = 1.0 + (double)params.inputGain * (double)params.inputGain * 39.0; // 0–40dB
        double v = (double)x * inputGain;
        const double dry = v;

        // === Sidechain: peak detection ===
        const double absV = std::abs(v);

        // attack/release time constants (logarithmic scale)
        const double attMs  = kAttackMinUs * 0.001
                            + (double)params.attack * (double)params.attack
                              * (kAttackMaxUs - kAttackMinUs) * 0.001;
        const double relMs  = kReleaseMinMs
                            + (double)params.release * (double)params.release
                              * (kReleaseMaxMs - kReleaseMinMs);

        const double attAlpha = msToAlpha(attMs);
        const double relAlpha = msToAlpha(relMs);

        if (absV > st.envelope)
            st.envelope += attAlpha * (absV - st.envelope);
        else
            st.envelope += relAlpha * (absV - st.envelope);

        // === Gain calculation ===
        // FET compressor has fixed threshold (~-30dBu); input gain drives signal into threshold
        const double envDb = (st.envelope > 1e-10)
            ? 20.0 * std::log10(st.envelope) : -200.0;
        const double threshDb = kThresholdDb;

        double gainDb = 0.0;
        if (envDb > threshDb)
        {
            double overDb = envDb - threshDb;
            double ratio = getRatio(params.ratio);

            if (params.ratio == (int)Ratio::All)
            {
                // All-Buttons: nonlinear compression (low level = expansion, high level = limiting)
                // unique response curve → distinctive compression feel
                double allGr = overDb * (1.0 - 1.0 / ratio);
                // negative ratio region (low-level boost)
                if (overDb < 6.0)
                    allGr *= 0.5 + overDb / 12.0;
                gainDb = -allGr;
            }
            else
            {
                // standard ratio: soft-knee compression
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

        // === JFET VCA nonlinearity (delegated to JFET_Primitive) ===
        v *= gain;
        v = jfet.vcaNonlinearity(v, gain);

        // === Output transformer coloration (light core saturation) ===
        {
            static constexpr double kSatLevel = 2.0;
            xfmrCoreSatState += (v - xfmrCoreSatState) * 0.001;
            if (std::abs(xfmrCoreSatState) > kSatLevel)
            {
                double amt = (std::abs(xfmrCoreSatState) - kSatLevel) * 0.2;
                v -= (xfmrCoreSatState > 0.0 ? amt : -amt);
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
    static constexpr double kSoftKneeDb   = 4.0;
    static constexpr double kAttackMinUs  = 20.0;
    static constexpr double kAttackMaxUs  = 800.0;
    static constexpr double kReleaseMinMs = 50.0;
    static constexpr double kReleaseMaxMs = 1100.0;
    static constexpr double kMinGain      = 0.001;

    // === Component layer (Parts) ===
    JFET_Primitive jfet;             // 2N5457
    double xfmrCoreSatState = 0.0;   // output transformer saturation state

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
