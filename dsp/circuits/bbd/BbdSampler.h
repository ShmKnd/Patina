#pragma once
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"

// BBD sampler — reproduces real hardware sample & hold behavior
class BbdSampler
{
public:
    BbdSampler() noexcept : sampleRate(PartsConstants::defaultSampleRate) {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = sr;
        holdCounter.assign((size_t)numChannels, 0);   // per-channel hold counter
        holdValue.assign((size_t)numChannels, 0.0f);  // per-channel hold value
    }
    void prepare(const patina::ProcessSpec& spec) noexcept { prepare(spec.numChannels, spec.sampleRate); }
    void reset() noexcept
    {
        std::fill(holdCounter.begin(), holdCounter.end(), 0);
        std::fill(holdValue.begin(), holdValue.end(), 0.0f);
    }

    // real-time safe: no memory allocation, no locks. RNG provided by caller
    inline float processSample(int channel, float inputPlusFeedback, double step,
                               bool emulateBbd, bool enableAging, double clockJitterStd,
                               std::minstd_rand& rng, std::normal_distribution<double>& normalDist) noexcept
    {
        if (!emulateBbd)
            return inputPlusFeedback;

        if (channel < 0 || (size_t)channel >= holdValue.size())
            return inputPlusFeedback;

        int holdSamples = static_cast<int>(std::round(step));
        if (holdSamples < 1) holdSamples = 1;

        if (enableAging && clockJitterStd > 0.0)
        {
            double jitter = normalDist(rng) * clockJitterStd; // positive/negative jitter value
            double factor = 1.0 + jitter;
            if (!std::isfinite(factor) || factor <= 0.1) factor = 0.1; // clamped at minimum 0.1
            holdSamples = std::max(1, static_cast<int>(std::round(step * factor)));
        }

        // when counter expires, acquire new sample and reload
        if (holdCounter[(size_t)channel] <= 0)
        {
            holdValue[(size_t)channel] = inputPlusFeedback;
            holdCounter[(size_t)channel] = holdSamples;
        }

        float out = holdValue[(size_t)channel];
        --holdCounter[(size_t)channel];
        return out;
    }

private:
    double sampleRate;                 // sample rate
    std::vector<int> holdCounter;      // per-channel hold period counter
    std::vector<float> holdValue;      // per-channel hold value
};
