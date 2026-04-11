#pragma once
#include <vector>
#include <random>
#include "../../core/AudioCompat.h"
#include "BbdFeedback.h"
#include "../../constants/PartsConstants.h"

// feedback signal processing bus — with saturation and noise addition
struct BbdNoise {
    // Feedback signal shaping (op-amp saturation, noise gain application)
    static inline float processFeedback(
        int ch,
        float rawFb,
        BbdFeedback& fbMod,
        std::minstd_rand& rng,
        std::normal_distribution<double>& normalDist,
        double opAmpNoiseGain,
        double bbdNoiseLevel,
        double sampleRate,
        bool highVoltageMode = false,
        double capacitanceScale = 1.0)
    {
        patina::compat::ignoreUnused(rng, normalDist, opAmpNoiseGain, bbdNoiseLevel, sampleRate);
        return fbMod.process(ch, rawFb, PartsConstants::emulateOpAmpSaturation, rng, normalDist, opAmpNoiseGain, bbdNoiseLevel, sampleRate, highVoltageMode, capacitanceScale);
    }

    // noise addition to write sample after S/H (op-amp thermal noise, BBD HF noise, aging white noise)
    static inline void addNoiseAfterSample(
        int ch,
        float& writeSample,
        bool enableAging,
        double opAmpNoiseGain,
        double bbdNoiseLevel,
        double bbdWhiteNoiseStd,
        double sampleRate,
        std::minstd_rand& rng,
        std::normal_distribution<double>& normalDist,
        std::vector<double>& bbdNoiseHpXPrev,
        std::vector<double>& bbdNoiseHpYPrev)
    {
        if (enableAging && opAmpNoiseGain > 1.0) {
            double n = normalDist(rng) * PartsConstants::thermalNoise * (opAmpNoiseGain - 1.0);
            writeSample += static_cast<float>(n);
        }
        if (bbdNoiseLevel > 0.0) {
            double rawN = normalDist(rng) * bbdNoiseLevel;
            const double fc_hp = PartsConstants::bbdNoiseHpFc;
            const double RC = 1.0 / (2.0 * patina::compat::MathConstants<double>::pi * fc_hp);
            const double dt = 1.0 / sampleRate;
            const double hpAlpha = RC / (RC + dt); // RC high-pass filter coefficient
            double prevX = bbdNoiseHpXPrev[(size_t)ch];
            double prevY = bbdNoiseHpYPrev[(size_t)ch];
            double hp = hpAlpha * (prevY + rawN - prevX);
            bbdNoiseHpXPrev[(size_t)ch] = rawN;
            bbdNoiseHpYPrev[(size_t)ch] = hp;
            const double noiseInjectScale = 4.0; // BBD noise injection gain (for matching hardware characteristics)
            writeSample += static_cast<float>(hp * noiseInjectScale);
        }
        if (bbdWhiteNoiseStd > 0.0) {
            writeSample += static_cast<float>(normalDist(rng) * bbdWhiteNoiseStd);
        }
    }
};
