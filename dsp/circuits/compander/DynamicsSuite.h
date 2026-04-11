#pragma once
#include <vector>
#include <atomic>
#include <cmath>
#include "../../core/AudioCompat.h"
#include <algorithm>
#include "../../constants/PartsConstants.h"

// Dynamics suite — HF-weighted makeup gain control
struct DynamicsSuite {
    // Integrated processing of HF-weighted aging-compensated makeup gain
    static inline void process(
        std::vector<float>& delayedOut,
        int numChannels,
        double sampleRate,
        double targetWetRms,
        // HP state for HF RMS measurement (per-channel history)
        std::vector<double>& hfHpXPrev,
        std::vector<double>& hfHpYPrev,
        // Smoothing and state parameters
        double delayedRmsAlpha,
        double hfRmsAlpha,
        double makeupSmoothing,
        // Input/output state variables
        double& smoothedDelayedRmsSq,
        double& smoothedDelayedHfRmsSq,
        double& currentMakeupGain,
        std::atomic<double>& baselineDelayedRms,
        std::atomic<double>& baselineDelayedHfRms,
        bool enableAging,
        bool& prevEnableAging
    )
    {
        // Full-band RMS measurement (overall power of delayed signal)
        double sumSq = 0.0;
        for (int ch = 0; ch < numChannels && (size_t) ch < delayedOut.size(); ++ch) {
            double v = (double) delayedOut[(size_t)ch];
            sumSq += v * v;
        }
        double instMs = (numChannels > 0) ? (sumSq / std::max(1, numChannels)) : 0.0;
        smoothedDelayedRmsSq = (1.0 - delayedRmsAlpha) * smoothedDelayedRmsSq + delayedRmsAlpha * instMs;
        double currentRms = std::sqrt(smoothedDelayedRmsSq + 1e-16); // 1e-16 to prevent divide by zero

        // Explicitly correct full-band level after compander to target RMS
        if (enableAging && targetWetRms > 1.0e-8 && currentRms > 1.0e-8)
        {
            const double targetGain = std::clamp(targetWetRms / currentRms, 0.25, 64.0);
            if (std::fabs(targetGain - 1.0) > 1.0e-4)
            {
                for (int ch = 0; ch < numChannels && (size_t) ch < delayedOut.size(); ++ch)
                    delayedOut[(size_t)ch] = (float) (delayedOut[(size_t)ch] * targetGain);
                smoothedDelayedRmsSq *= (targetGain * targetGain);
                currentRms *= targetGain;
            }
        }

        // HF RMS measurement — extract HF content via 1-pole HP per channel
        const double hfFc = PartsConstants::bbdHfCompFc; // HF compensation filter frequency
        const double RC_hf = 1.0 / (2.0 * patina::compat::MathConstants<double>::pi * hfFc);
        const double dt = 1.0 / std::max(1.0, sampleRate);
        const double hpA = RC_hf / (RC_hf + dt); // 1-pole HP filter coefficient
        if ((int)hfHpXPrev.size() != numChannels) hfHpXPrev.assign((size_t)numChannels, 0.0);
        if ((int)hfHpYPrev.size() != numChannels) hfHpYPrev.assign((size_t)numChannels, 0.0);
        double sumHfSq = 0.0;
        for (int ch = 0; ch < numChannels && (size_t) ch < delayedOut.size(); ++ch) {
            double x = (double) delayedOut[(size_t)ch];
            double prevX = hfHpXPrev[(size_t)ch];
            double prevY = hfHpYPrev[(size_t)ch];
            double y = hpA * (prevY + x - prevX); // 1-pole HP filter processing
            hfHpXPrev[(size_t)ch] = x;
            hfHpYPrev[(size_t)ch] = y;
            sumHfSq += y * y;
        }
        double instHfMs = (numChannels > 0) ? (sumHfSq / std::max(1, numChannels)) : 0.0;
        smoothedDelayedHfRmsSq = (1.0 - hfRmsAlpha) * smoothedDelayedHfRmsSq + hfRmsAlpha * instHfMs;
        double currentHfRms = std::sqrt(smoothedDelayedHfRmsSq + 1e-16);

        // Baseline update at aging state boundary
        if (enableAging && !prevEnableAging) {
            baselineDelayedRms.store(currentRms);
            baselineDelayedHfRms.store(currentHfRms);
        }
        if (!enableAging && prevEnableAging) {
            baselineDelayedRms.store(currentRms);
            baselineDelayedHfRms.store(currentHfRms);
            currentMakeupGain = 1.0; // Reset gain when aging is disabled
        }

        // Target gain calculation (full-band and HF separate)
        double targetGainFull = 1.0;
        double baseR = baselineDelayedRms.load();
        if (enableAging && currentRms > 1e-12 && baseR > 1e-12) {
            targetGainFull = baseR / currentRms; // Calculate gain from baseline ratio
            targetGainFull = std::clamp(targetGainFull, 0.75, 2.5); // 0.75-2.5x limit
        }
        double targetGainHf = 1.0;
        double baseHf = baselineDelayedHfRms.load();
        if (enableAging && currentHfRms > 1e-14 && baseHf > 1e-14) {
            targetGainHf = baseHf / currentHfRms; // HF content baseline ratio
            targetGainHf = std::clamp(targetGainHf, 0.85, 2.2); // 0.85-2.2x limit（narrower range for HF）
        }

        // Apply combined target gain and smoothing
        double combinedTargetGain = 1.0;
        if (enableAging) {
            const double hfWeight = 0.7; // 0.7Combined gain weighted toward HF priority
            combinedTargetGain = hfWeight * targetGainHf + (1.0 - hfWeight) * targetGainFull;
            combinedTargetGain = std::clamp(combinedTargetGain, 0.75, 2.5); // Final limit
        }

        if (!enableAging) {
            currentMakeupGain = 1.0;
        } else {
            currentMakeupGain = (1.0 - makeupSmoothing) * currentMakeupGain + makeupSmoothing * combinedTargetGain;
            if (std::fabs(currentMakeupGain - 1.0) > 1e-5) { // 1e-5Apply only significant gain changes above threshold
                for (int ch = 0; ch < numChannels && (size_t) ch < delayedOut.size(); ++ch)
                    delayedOut[(size_t)ch] = (float) (delayedOut[(size_t)ch] * currentMakeupGain);
            }
        }

        prevEnableAging = enableAging; // update previous state
    }
};
