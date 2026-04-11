#pragma once

#include "../../core/AudioCompat.h"
#include <algorithm>
#include "../../constants/PartsConstants.h"

// Lightweight delay line readout utility (non-owning buffer)
// Mirrors the PluginProcessor readout functionality, providing ring-buffer access
// No buffer ownership: receives buffer and write position from external source
struct DelayLineView {
    // Readout from base-rate delay buffer
    static float readFromDelay(const patina::compat::AudioBuffer<float>& buffer,
                               int writePos,
                               int channel,
                               double delaySamples,
                               int stages,
                               bool emulateBBD)
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0) return 0.0f;
        if (channel < 0 || channel >= buffer.getNumChannels()) return 0.0f;

        // Calculate latest write index
        double wp = static_cast<double>(writePos) - 1.0;
        if (!std::isfinite(wp)) wp = 0.0;
        if (wp < 0.0) wp += numSamples;

        double readPos = wp - delaySamples;
        if (!std::isfinite(readPos)) return 0.0f;
        while (readPos < 0.0) readPos += numSamples;
        if (readPos >= numSamples) readPos = std::fmod(readPos, static_cast<double>(numSamples));

        if (!emulateBBD)
            return linearRead(buffer, channel, readPos);

        stages = static_cast<int>(std::clamp(stages, 1, PartsConstants::bbdStagesDefault)); // 1-bbdStagesDefaultstage limiting (DMM compatible)
        const double step = std::max(1.0, delaySamples / static_cast<double>(stages));
        const double qpos = std::floor(readPos / step) * step; // S&H quantization position

        const float quant = linearRead(buffer, channel, qpos);
        const float line  = linearRead(buffer, channel, readPos);

        double blend = 0.6 + 0.02 * (step - 2.0); // Blend coefficient according to step size
        blend = std::clamp(blend, 0.25, 0.9);   // 0.25-0.9range limit
        return static_cast<float>(blend * quant + (1.0 - blend) * line);
    }

    // Readout from oversample delay buffer (no stage specification, linear interpolation)
    static float readFromOversampleDelay(const patina::compat::AudioBuffer<float>& osBuffer,
                                         int oversampleWritePos,
                                         int channel,
                                         double delaySamples,
                                         bool emulateBBD)
    {
        patina::compat::ignoreUnused(emulateBBD); // Current OS path uses linear readout
        const int numSamples = osBuffer.getNumSamples();
        if (numSamples <= 0) return 0.0f;
        if (channel < 0 || channel >= osBuffer.getNumChannels()) return 0.0f;

        double wp = static_cast<double>(oversampleWritePos) - 1.0;
        if (!std::isfinite(wp)) wp = 0.0;
        if (wp < 0.0) wp += numSamples;

        double readPos = wp - delaySamples;
        if (!std::isfinite(readPos)) return 0.0f;
        while (readPos < 0.0) readPos += numSamples;
        if (readPos >= numSamples) readPos = std::fmod(readPos, static_cast<double>(numSamples));

        return linearRead(osBuffer, channel, readPos);
    }

private:
    // Linear interpolation readout from ring buffer
    static float linearRead(const patina::compat::AudioBuffer<float>& buffer, int ch, double pos)
    {
        const int bufSize = buffer.getNumSamples();
        if (bufSize <= 0) return 0.0f;
        const int i1 = static_cast<int>(std::floor(pos));
        const int i2 = (i1 + 1) % bufSize; // Ring boundary handling
        const double frac = pos - std::floor(pos);
        const float s1 = buffer.getSample(ch, i1);
        const float s2 = buffer.getSample(ch, i2);
        return static_cast<float>(s1 + (s2 - s1) * frac);
    }
};
