#pragma once

#include "../../constants/PartsConstants.h"
#include "../../core/AudioCompat.h"
#include "../../core/ProcessSpec.h"
#include <vector>

// Per-channel tone IIR low-pass filter bank — with analog RC circuit emulation
// Coefficient update management based on tone parameters and optional analog RC circuit emulation
class ToneFilter {
public:
    void prepare(double sampleRate, int channels, int maxBlock)
    {
        sr = sampleRate;
        resizefilter(channels);
        setDefaultCutoff();
        reset();
    }

    void prepare(const patina::ProcessSpec& spec)
    {
        prepare(spec.sampleRate, spec.numChannels, spec.maxBlockSize);
    }

    void ensureChannels(int channels, int maxBlock)
    {
        if ((int)filters.size() != channels)
            resizefilter(channels);
    }

    void reset()
    {
        for (auto& f : filters) f.reset();
    }

    // safe default cutoff setting (used during prepare)
    void setDefaultCutoff(float cutoffHz = 2000.0f)
    {
        if (sr <= 1.0) return;
        auto coeffs = patina::compat::dsp::IIRCoefficients<float>::makeLowPass(sr, cutoffHz);
        for (auto& f : filters) f.setCoefficients(coeffs);
    }

    // fixed cutoff direct setting (for legacy code compatibility)
    void updateToneFilter(float cutoffHz = 2000.0f)
    {
        setDefaultCutoff(cutoffHz);
    }

    // Update cutoff from tone parameters, RC circuit emulation, and aging scale
    void updateToneFilterIfNeeded(float toneParam, double agingCapScale, bool emulateToneRC)
    {
        if (sr <= 1.0) return;
        float cutoff = static_cast<float>(PartsConstants::toneFreqMin) + toneParam * static_cast<float>(PartsConstants::toneFreqMax - PartsConstants::toneFreqMin);
        cutoff = static_cast<float>(std::max(80.0, static_cast<double>(cutoff) * agingCapScale));
        if (emulateToneRC)
        {
            double potRes = PartsConstants::R_tonePot * (1.0 - static_cast<double>(toneParam));
            potRes = std::max(PartsConstants::tonePotMinOhm, potRes);
            double rcF = 1.0 / (2.0 * patina::compat::MathConstants<double>::pi * potRes * PartsConstants::C_tone);
            rcF = std::clamp(rcF, PartsConstants::toneRcMinHz, PartsConstants::toneRcMaxHz);
            const double blend = PartsConstants::toneRcLinearBlend;
            cutoff = static_cast<float>(blend * rcF + (1.0 - blend) * (PartsConstants::toneFreqMin + toneParam * (PartsConstants::toneFreqMax - PartsConstants::toneFreqMin)));
        }
        auto coeffs = patina::compat::dsp::IIRCoefficients<float>::makeLowPass(sr, cutoff);
        for (auto& f : filters) f.setCoefficients(coeffs);
    }
    
    // Advanced Tuning: Set cutoff frequency directly (ignoring tone parameters)
    void updateToneFilterWithCutoff(float cutoffHz, double agingCapScale)
    {
        if (sr <= 1.0) return;
        float cutoff = static_cast<float>(std::max(80.0, static_cast<double>(cutoffHz) * agingCapScale));
        auto coeffs = patina::compat::dsp::IIRCoefficients<float>::makeLowPass(sr, cutoff);
        for (auto& f : filters) f.setCoefficients(coeffs);
    }

    float processSample(int ch, float x)
    {
        if (filters.empty()) return x;
        const int idx = std::clamp(ch, 0, (int)filters.size() - 1); // channel boundary clamping
        return filters[(size_t)idx].processSample(x);
    }
    // block processing API
    void processBlock(float* const* io, int numChannels, int numSamples)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = processSample(ch, io[ch][i]);
    }
private:
    void resizefilter(int channels)
    {
        filters.clear();
        filters.resize((size_t)std::max(1, channels));
        for (auto& f : filters)
            f.reset();
    }

    double sr { 0.0 };                                           // sample rate
    std::vector<patina::compat::dsp::IIRFilter<float>> filters;  // per-channel IIR filter array
};
