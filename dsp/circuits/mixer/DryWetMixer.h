#pragma once
#include <cmath>
#include <algorithm>
#include "../../core/AudioCompat.h"

// DryWetMixer — Equal-power crossfade (zero external DSP dependency)
struct DryWetMixer {
    static constexpr float kWetMakeupGain = 1.0;

    static constexpr size_t kLutSize = 256;
    static constexpr float kLutScale = static_cast<float>(kLutSize - 1);

    struct CosSinLUT {
        float cosTable[kLutSize];
        float sinTable[kLutSize];

        CosSinLUT() {
            for (size_t i = 0; i < kLutSize; ++i) {
                double angle = static_cast<double>(i) / static_cast<double>(kLutSize - 1)
                             * (3.14159265358979323846 * 0.5);
                cosTable[i] = static_cast<float>(std::cos(angle));
                sinTable[i] = static_cast<float>(std::sin(angle));
            }
        }
    };

    static inline const CosSinLUT& getCosSinLUT() noexcept {
        static const CosSinLUT lut;
        return lut;
    }

    static inline void equalPowerGainsFast(double mix01, float& gDry, float& gWet) noexcept {
        const float m = static_cast<float>(std::clamp(mix01, 0.0, 1.0));
        const size_t idx = static_cast<size_t>(m * kLutScale + 0.5f);
        const size_t clampedIdx = std::min(idx, kLutSize - 1);
        const auto& lut = getCosSinLUT();
        gDry = lut.cosTable[clampedIdx];
        gWet = lut.sinTable[clampedIdx];
    }

    static inline void equalPowerGains(double mix01, float& gDry, float& gWet) noexcept {
        const double m = std::clamp(mix01, 0.0, 1.0);
        gDry = (float) std::cos(m * patina::compat::MathConstants<double>::halfPi);
        gWet = (float) std::sin(m * patina::compat::MathConstants<double>::halfPi);
    }

    static inline float equalPowerMixVolt(float dryV, float wetV, double mix01, float wetMakeupBase = kWetMakeupGain) noexcept {
        float gDry = 1.0f, gWet = 0.0f;
        equalPowerGainsFast(mix01, gDry, gWet);
        const float m = (float) std::clamp(mix01, 0.0, 1.0);
        const float wetMakeup = 1.0f + (wetMakeupBase - 1.0f) * m;
        return dryV * gDry + (wetV * wetMakeup) * gWet;
    }
};
