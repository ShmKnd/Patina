#pragma once
#include <vector>
#include <algorithm>
#include "../../core/AudioCompat.h"

// stereo image processing — stereo widening via Mid/Side matrix (SIMD/NEON optimized)
struct StereoImage {
    // SIMD-optimized: Mid/Side widening (direct float reference processing)
    static inline void widenEqualPowerSIMD(float& L, float& R, float depth01) noexcept {
        if (depth01 <= 0.0f) return; // skip processing if depth is 0

        const float invSqrt2 = 0.7071067811865475244f; // 1/√2
        
        // Mid/Side calculation
        auto invSqrt2Vec = patina::compat::SIMDRegister<float>(invSqrt2);
        
        // M = (L+R)/√2, S = (L-R)/√2
        float M = (L + R) * invSqrt2;
        float S = (L - R) * invSqrt2;
        
        // widening (0.6 at depth=0, 0.85 at depth=1)
        float width = 0.6f + 0.25f * depth01;
        width = std::clamp(width, 0.0f, 1.0f);
        S *= width;
        
        // L/R reconstruction (SIMD parallelized)
        float ms[4] = { M + S, M - S, 0.0f, 0.0f };
        auto msVec = patina::compat::SIMDRegister<float>::fromRawArray(ms);
        msVec = msVec * invSqrt2Vec; // (M+S)/√2, (M-S)/√2
        
        float output[4];
        msVec.copyToRawArray(output);
        L = output[0];
        R = output[1];
    }

    // equal-power Mid/Side widening (with safety clamping) — vector version (backward compatibility)
    // depth01: 0–1 range. In existing spec, only Side component is expanded by 0.5+0.5*depth
    static inline void widenEqualPower(std::vector<float>& stereoVolt, float depth01) noexcept {
        if (stereoVolt.size() < 2) return; // skip processing for mono or insufficient channels
        if (depth01 <= 0.0f) return;       // skip processing if depth is 0

        // call SIMD version
        widenEqualPowerSIMD(stereoVolt[0], stereoVolt[1], depth01);
    }
};
