#pragma once
#include "../../core/AudioCompat.h"
#include <algorithm>

// delay core — basic delay calculation and chorus modulation
struct BbdClock {
    // effective delay with chorus read position modulation (minimum 1.5 samples guaranteed)
    static inline double effectiveDelaySamples(double baseDelaySamples,
                                               double chorusDepthSamples,
                                               double sinvVal) noexcept
    {
        double d = baseDelaySamples + chorusDepthSamples * sinvVal;
        return std::max(1.5, d); // avoid interpolation errors below 1.5 samples
    }

    // BBD step calculation reflecting clock scale modulation by triangle wave
    // larger chorusEff produces stronger pitch wobble (nonlinear curve)
    static inline double stepWithClock(double baseStep,
                                       double triVal,
                                       float chorusEff,
                                       double kMaxClockModPct) noexcept
    {
        if (chorusEff <= 0.0f) return baseStep;
        
        // nonlinear scaling per Chorus knob (wobbles more strongly toward the right)
        // 0.0–0.5: moderate wobble (0–50% depth)
        // 0.5–1.0: strong wobble (50–100% depth, accelerating increase)
        double effectiveChorусEff = chorusEff * chorusEff;  // quadratic curve (e.g.: 0.7→0.49, 0.9→0.81)
        
        const double depthPct = (double) effectiveChorусEff * kMaxClockModPct;
        // expanded clock scale range (accommodating aggressive modulation of classic BBD delays)
        // more extreme pitch wobble in the 0.4–1.6x range
        double clockScale = std::clamp(1.0 + depthPct * triVal, 0.4, 1.6);
        double stepEff = baseStep / clockScale;
        if (stepEff < 1.0) stepEff = 1.0; // minimum 1-sample step guaranteed
        return stepEff;
    }

    // convert chorus depth from ms to sample count (with overmodulation prevention)
    static inline double chorusDepthSamples(double chorusEff,
                                            double timeMs,
                                            double sampleRate,
                                            double depthMsMax,
                                            double fractionCap = 0.45) noexcept
    {
        double depthMs = (double) chorusEff * depthMsMax;
        depthMs = std::min(depthMs, timeMs * fractionCap); // overmodulation suppressed at 45% of delay time
        return depthMs * 0.001 * sampleRate; // ms → sample count conversion
    }

    // dynamic scaling of chorus depth: auto-adjusts to delay time and feedback
    // shallow at short delays, deep at long delays, moderate at high feedback
    static inline double chorusDepthSamplesDynamic(double chorusEff,
                                                   double timeMs,
                                                   double feedback,
                                                   double sampleRate,
                                                   double depthMsMax) noexcept
    {
        if (chorusEff <= 0.0) return 0.0;
        
        // depth scale per delay time (shallow at short delays)
        double timeScale = 1.0;
        if (timeMs < 100.0) {
            // 10-100ms: 0.3-1.0range linear interpolation
                timeScale = 0.3 + 0.7 * std::clamp((timeMs - 10.0) / 90.0, 0.0, 1.0);
        } else if (timeMs < 300.0) {
            // 100–300ms: 1.0–1.2 range (standard to slightly deeper)
            timeScale = 1.0 + 0.2 * std::clamp((timeMs - 100.0) / 200.0, 0.0, 1.0);
        } else {
            // 300ms+: fixed at 1.2 (deep but not excessive)
            timeScale = 1.2;
        }
        
        // scale per feedback amount (moderate at high FB)
        double fbScale = 1.0;
        if (feedback > 0.5) {
            // FB 0.5–1.0: attenuates to 0.6 (prevents overmodulation/pitch shift at high FB)
            fbScale = 1.0 - 0.4 * std::clamp((feedback - 0.5) / 0.5, 0.0, 1.0);
        }
        
        // final depth calculation
        double effectiveDepthMs = chorusEff * depthMsMax * timeScale * fbScale;
        
        // safety upper limit: up to 35% of delay time (more conservative)
        effectiveDepthMs = std::min(effectiveDepthMs, timeMs * 0.35);
        
        return effectiveDepthMs * 0.001 * sampleRate; // ms → sample count conversion
    }
};
