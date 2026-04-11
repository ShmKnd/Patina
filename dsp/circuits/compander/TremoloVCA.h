#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"

// Classic American amplifier-style analog tremolo / VCA model
// Bias tremolo: modulates push-pull stage bias (asymmetric crossover distortion)
// Optical (Vactrol): asymmetric LDR response (fast attack, slow release)
// Simple VCA: linear gain control
class TremoloVCA
{
public:
    enum class Mode { Bias = 0, Optical = 1, VCA = 2 };

    struct Params
    {
        float depth = 0.5f;         // Tremolo depth 0.0–1.0
        float lfoValue = 0.0f;      // External LFO value -1.0–1.0 (from AnalogLfo etc.)
        int mode = 0;               // 0=Bias, 1=Optical, 2=VCA
        bool stereoPhaseInvert = false; // Stereo: invert ch1 LFO
    };

    TremoloVCA() noexcept = default;

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        opticalState.assign(nCh, 0.5);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(opticalState.begin(), opticalState.end(), 0.5);
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)opticalState.size() - 1);
        const double depth = std::clamp((double)params.depth, 0.0, 1.0);

        // Apply stereo phase inversion to LFO value
        double lfo = params.lfoValue;
        if (params.stereoPhaseInvert && (channel & 1))
            lfo = -lfo;

        // LFO -> gain conversion (per mode)
        double gain;

        switch (params.mode)
        {
            case 0: // Bias tremolo (classic American amplifier)
            {
                // Asymmetric modulation: push-pull stage bias shift
                // Strong attenuation on positive half, slight boost on negative half
                const double modSig = lfo * depth;
                // Asymmetric curve (even harmonic generation)
                if (modSig >= 0.0)
                    gain = 1.0 - modSig * 0.8; // Positive: strong attenuation
                else
                    gain = 1.0 - modSig * 0.3; // Negative: slight boost
                gain = std::clamp(gain, 0.0, 1.3);
                break;
            }

            case 1: // Optical (Vactrol)
            {
                // Asymmetric LDR time constants: attack ~1ms, release ~10ms
                const double target = 0.5 + lfo * depth * 0.5; // 0.0–1.0
                double& os = opticalState[ch];
                const double attMs = 1.0;
                const double relMs = 10.0;
                const double alpha = (target > os)
                    ? (1.0 - std::exp(-1.0 / (sampleRate * attMs * 0.001)))
                    : (1.0 - std::exp(-1.0 / (sampleRate * relMs * 0.001)));
                os += alpha * (target - os);
                // Log scale (LDR has logarithmic response)
                gain = os * os; // Squared for logarithmic approximation
                gain = 0.2 + gain * 0.8; // Minimum gain 0.2 (does not fully mute)
                break;
            }

            default: // VCA (linear)
            {
                gain = 1.0 - depth * (1.0 - (lfo * 0.5 + 0.5));
                gain = std::clamp(gain, 0.0, 1.0);
                break;
            }
        }

        return (float)((double)x * gain);
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

private:
    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<double> opticalState; // Vactrol LDR tracking state
};
