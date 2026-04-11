#pragma once
#include <vector>
#include "../../core/AudioCompat.h"

// Oversample path processing — 2x/4x processing and linear interpolation
struct OversamplePath {
    // Allocate OS buffer to requested size and initialize (reset write position and history)
    static inline void ensureBuffer(
        patina::compat::AudioBuffer<float>& osBuf,
        int& osWritePos,
        std::vector<float>& lastInputSample,
        int numChannels,
        int desiredSamples)
    {
        if (osBuf.getNumChannels() != numChannels || osBuf.getNumSamples() < desiredSamples) {
            osBuf.setSize(numChannels, desiredSamples);
            osBuf.clear();
            osWritePos = 0;
            lastInputSample.assign((size_t)numChannels, 0.0f); // Per-channel previous input value
        }
    }

    // Safe write to OS buffer (with bounds checking)
    static inline void write(patina::compat::AudioBuffer<float>& osBuf, int ch, int osWritePos, float value) noexcept {
        if (osBuf.getNumSamples() > 0 && osWritePos >= 0 && osWritePos < osBuf.getNumSamples())
            osBuf.setSample(ch, osWritePos, value);
    }

    // Advance OS buffer write position (ring boundary handling)
    static inline void advance(patina::compat::AudioBuffer<float>& osBuf, int& osWritePos) noexcept {
        if (++osWritePos >= osBuf.getNumSamples()) osWritePos = 0;
    }

    // Sub-sample linear interpolation (calculates intermediate value from previous and current base-rate input)
    static inline float lerpSubsample(float prev, float now, int s, int osFactor) noexcept {
        const double t = (double)(s + 1) / (double) osFactor; // Interpolation coefficient: in 0.0-1.0 range
        return prev + (float) t * (now - prev);
    }
};
