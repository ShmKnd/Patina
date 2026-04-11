#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"

// Classic envelope filter-style envelope follower
// RMS / peak detection → control voltage output
// Used for auto-wah, dynamic EQ, sidechain control
class EnvelopeFollower
{
public:
    enum class DetectionMode { Peak = 0, RMS = 1 };

    struct Params
    {
        float attackMs = 5.0f;      // Attack time (ms)
        float releaseMs = 50.0f;    // Release time (ms)
        float sensitivity = 1.0f;   // Sensitivity 0.1–10.0
        int mode = 0;               // 0=Peak, 1=RMS
    };

    EnvelopeFollower() noexcept = default;

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        envState.assign(nCh, 0.0);
        rmsState.assign(nCh, 0.0);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(envState.begin(), envState.end(), 0.0);
        std::fill(rmsState.begin(), rmsState.end(), 0.0);
    }

    // Returns envelope value (normalized to 0.0–1.0 range)
    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)envState.size() - 1);

        // Time constants → IIR coefficients
        const double attAlpha = msToAlpha(std::max(0.1f, params.attackMs));
        const double relAlpha = msToAlpha(std::max(0.1f, params.releaseMs));

        const double sensitivity = std::clamp((double)params.sensitivity, 0.1, 10.0);
        double input;

        if (params.mode == 1)
        {
            // RMS Detection
            const double sq = (double)x * (double)x * sensitivity * sensitivity;
            rmsState[ch] += attAlpha * (sq - rmsState[ch]);
            input = std::sqrt(std::max(0.0, rmsState[ch]));
        }
        else
        {
            // Peak detection
            input = std::abs((double)x) * sensitivity;
        }

        // Peak follower (asymmetric attack/release smoothing)
        double& env = envState[ch];
        if (input > env)
            env += attAlpha * (input - env);
        else
            env += relAlpha * (input - env);

        return (float)std::clamp(env, 0.0, 1.0);
    }

    // Passes input signal through and writes envelope to controlOut
    void processBlock(const float* const* input, float* const* output,
                      float* controlOut,  // [numSamples] Envelope output
                      int numChannels, int numSamples, const Params& params) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float maxEnv = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                output[ch][i] = input[ch][i];
                float env = process(ch, input[ch][i], params);
                if (env > maxEnv) maxEnv = env;
            }
            if (controlOut) controlOut[i] = maxEnv;
        }
    }

    // Get current envelope value (for UI display, etc.)
    float getEnvelope(int channel) const noexcept
    {
        if (channel < 0 || (size_t)channel >= envState.size()) return 0.0f;
        return (float)envState[(size_t)channel];
    }

private:
    inline double msToAlpha(float ms) const noexcept
    {
        if (ms <= 0.0f) return 1.0;
        return 1.0 - std::exp(-1.0 / (sampleRate * ms * 0.001));
    }

    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<double> envState;   // Envelope detector state
    std::vector<double> rmsState;   // Squared mean state for RMS
};
