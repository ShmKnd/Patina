#pragma once
#include <cmath>
#include <algorithm>
#include "DryWetMixer.h"

// DuckingMixer — Ducking mix specific to analog delay
struct DuckingMixer : DryWetMixer {
    static constexpr float kDuckingAmount = 0.01f;
    static constexpr float kDuckingMinFloor = 0.6f;
    static constexpr float kDuckingMixThresholdLow = 0.3f;
    static constexpr float kDuckingMixThresholdHigh = 0.7f;

    static constexpr float kAttackDuckingAmount = 0.05f;
    static constexpr float kAttackDetectHpFreq = 200.0f;
    static constexpr float kAttackEnvAttackTime = 0.001f;
    static constexpr float kAttackEnvReleaseTime = 0.050f;

    static inline float analogDuckingMix(float dryV, float wetV, double mix01, float wetMakeupBase = kWetMakeupGain, float duckingAmount = kDuckingAmount) noexcept {
        float gDry = 1.0f, gWet = 0.0f;
        equalPowerGainsFast(mix01, gDry, gWet);

        const float dryEnergy = std::fabs(dryV);

        float duckingScale = 0.0f;
        if (mix01 < kDuckingMixThresholdLow) {
            duckingScale = 0.0f;
        } else if (mix01 < kDuckingMixThresholdHigh) {
            const float range = kDuckingMixThresholdHigh - kDuckingMixThresholdLow;
            duckingScale = static_cast<float>((mix01 - kDuckingMixThresholdLow) / range);
        } else {
            const float range = 1.0f - kDuckingMixThresholdHigh;
            duckingScale = 1.0f - static_cast<float>((mix01 - kDuckingMixThresholdHigh) / range);
        }

        const float effectiveDucking = duckingAmount * duckingScale;
        const float duckingFactor = 1.0f - (effectiveDucking * dryEnergy * gWet);
        const float duckedWetGain = gWet * std::max(kDuckingMinFloor, duckingFactor);

        const float m = (float) std::clamp(mix01, 0.0, 1.0);
        const float wetMakeup = 1.0f + (wetMakeupBase - 1.0f) * m;

        return dryV * gDry + (wetV * wetMakeup) * duckedWetGain;
    }

    struct AttackDuckingState {
        float envelope = 0.0f;
        float hpZ1 = 0.0f;
        void reset() { envelope = 0.0f; hpZ1 = 0.0f; }
    };

    static inline float analogDuckingMixWithAttackDetection(
        AttackDuckingState& state,
        float dryV, float wetV, double mix01, double sampleRate,
        float wetMakeupBase = kWetMakeupGain,
        float duckingAmount = kAttackDuckingAmount) noexcept
    {
        float gDry = 1.0f, gWet = 0.0f;
        equalPowerGainsFast(mix01, gDry, gWet);

        const float hpAlpha = static_cast<float>(1.0 - std::exp(-patina::compat::MathConstants<double>::twoPi * kAttackDetectHpFreq / sampleRate));
        const float hpOut = dryV - state.hpZ1;
        state.hpZ1 += hpAlpha * (dryV - state.hpZ1);

        const float attackEnergy = std::fabs(hpOut);
        const float attackCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (kAttackEnvAttackTime * sampleRate)));
        const float releaseCoeff = static_cast<float>(1.0 - std::exp(-1.0 / (kAttackEnvReleaseTime * sampleRate)));

        if (attackEnergy > state.envelope)
            state.envelope += attackCoeff * (attackEnergy - state.envelope);
        else
            state.envelope += releaseCoeff * (attackEnergy - state.envelope);

        float duckingScale = 0.0f;
        if (mix01 < kDuckingMixThresholdLow) {
            duckingScale = 0.0f;
        } else if (mix01 < kDuckingMixThresholdHigh) {
            const float range = kDuckingMixThresholdHigh - kDuckingMixThresholdLow;
            duckingScale = static_cast<float>((mix01 - kDuckingMixThresholdLow) / range);
        } else {
            const float range = 1.0f - kDuckingMixThresholdHigh;
            duckingScale = 1.0f - static_cast<float>((mix01 - kDuckingMixThresholdHigh) / range);
        }

        const float effectiveDucking = duckingAmount * duckingScale;
        const float duckingFactor = 1.0f - (effectiveDucking * state.envelope * gWet);
        const float duckedWetGain = gWet * std::max(kDuckingMinFloor, duckingFactor);

        const float m = (float) std::clamp(mix01, 0.0, 1.0);
        const float wetMakeup = 1.0f + (wetMakeupBase - 1.0f) * m;

        return dryV * gDry + (wetV * wetMakeup) * duckedWetGain;
    }
};
