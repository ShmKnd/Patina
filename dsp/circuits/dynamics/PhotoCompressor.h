#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/PhotocellPrimitive.h"
#include "../../parts/TubeTriode.h"

// photo-coupled optical compressor emulation (classic optical compressor/limiter style)
// - T4B photocell (CdS LDR + electroluminescent panel) model
// - Program-dependent attack/release
// - 2 modes: Compress / Limit
// - Slight even harmonics from tube output stage
//
// 4-layer architecture:
//   Parts: PhotocellPrimitive (T4B) + TubeTriode (12AX7)
//   → Circuit: PhotoCompressor (SC + VCA + output stage)
class PhotoCompressor
{
public:
    enum class Mode : int
    {
        Compress = 0,   // soft knee, low ratio (~3:1)
        Limit    = 1    // hard knee, high ratio (~∞:1)
    };

    struct Params
    {
        float peakReduction = 0.5f;     // Gain reduction amount 0.0–1.0 (opto compressor peak reduction knob)
        float outputGain    = 0.5f;     // Makeup gain 0.0–1.0
        int   mode          = 0;        // 0=Compress, 1=Limit
        float mix           = 1.0f;     // Dry/Wet mix 0.0–1.0
    };

    PhotoCompressor() noexcept
        : outputTube(TubeTriode::T12AX7())
    {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        chState.resize(nCh);
        for (auto& st : chState) st = ChannelState{};
        photocells.resize(nCh, PhotocellPrimitive(PhotocellPrimitive::T4B()));
        for (auto& pc : photocells) pc.prepare(sampleRate);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& st : chState) st = ChannelState{};
        for (auto& pc : photocells) pc.reset();
    }

    // single sample processing
    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);
        auto& st = chState[ch];

        const double input = (double)x;
        const double absInput = std::abs(input);

        // === Sidechain: signal level detection ===
        const double peakRed = std::clamp((double)params.peakReduction, 0.0, 1.0);
        const double sensitivity = 0.5 + peakRed * 4.0; // Sensitivity scaling
        const double scLevel = absInput * sensitivity;

        // === T4B photocell model (delegated to PhotocellPrimitive) ===
        double attenuation = photocells[ch].process(scLevel);
        attenuation = std::clamp(attenuation, 0.0, 1.0);

        // compression characteristics by mode
        double gain;
        if (params.mode == (int)Mode::Limit)
        {
            // Limit: hard knee, aggressive compression
            gain = 1.0 / (1.0 + attenuation * 20.0);
        }
        else
        {
            // compression: soft knee, gentle compression
            gain = 1.0 / (1.0 + attenuation * 5.0);
        }

        // Gain application
        double output = input * gain;

        // === Tube output stage coloration (delegated to TubeTriode) ===
        output = outputTube.softSaturate(output);

        // === Makeup gain ===
        const double makeupGain = (double)params.outputGain * (double)params.outputGain * 4.0;
        output *= makeupGain;

        // === Dry/Wet Mix ===
        const double mix = std::clamp((double)params.mix, 0.0, 1.0);
        output = input * (1.0 - mix) + output * mix;

        // for GR meter
        st.lastGainReduction = gain;

        return (float)output;
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

    // Get gain reduction value (dB) (for UI display)
    float getGainReductionDb(int channel) const noexcept
    {
        if (channel < 0 || (size_t)channel >= chState.size()) return 0.0f;
        double gr = chState[(size_t)channel].lastGainReduction;
        if (gr <= 0.001) return -60.0f;
        return (float)(20.0 * std::log10(gr));
    }

private:
    // === Component layer (Parts) ===
    std::vector<PhotocellPrimitive> photocells;  // T4B (per-channel)
    TubeTriode outputTube;         // 12AX7

    struct ChannelState
    {
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
