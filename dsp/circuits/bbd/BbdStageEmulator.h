#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../constants/PartsConstants.h"
#include "../../core/AudioCompat.h"
#include "../../core/ProcessSpec.h"

// BBD inter-stage emulator — reproduces real hardware RC stage cascade
class BbdStageEmulator
{
public:
    struct Params
    {
        int stages = PartsConstants::bbdStagesDefault;
        double supplyVoltage = 9.0;
        bool enableAging = false;
        double ageYears = 0.0;
    };

    BbdStageEmulator() noexcept : sampleRate(PartsConstants::defaultSampleRate), bbdStageA(0.0), bbdStageA2(0.0), lastSimulatedStages(0) {}

    inline void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = sr;
        lastSimulatedStages = 1;
        cachedStages = 0;
        cachedStageScale = 1.0;
        bbdStageStates.clear();
        bbdStageStates.resize((size_t)numChannels);
        for (int ch = 0; ch < numChannels; ++ch)
            bbdStageStates[(size_t)ch].assign(1, 0.0f);
        bbdDAState.assign((size_t)numChannels, 0.0);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept { prepare(spec.numChannels, spec.sampleRate); }

    inline void reset() noexcept
    {
        for (auto& vec : bbdStageStates)
            std::fill(vec.begin(), vec.end(), 0.0f);
        std::fill(bbdDAState.begin(), bbdDAState.end(), 0.0);
    }

    // in-place processing of channel output vector (per sample)
    inline void process(std::vector<float>& samples, double /*step*/, int stages, double supplyVoltage, bool /*enableAging*/, double /*ageYears*/) noexcept
    {
        if (samples.empty()) return;

        double supplyNorm = std::clamp((supplyVoltage - PartsConstants::V_supplyMin) / (PartsConstants::V_supplyMax - PartsConstants::V_supplyMin), 0.0, 1.0);

        double targetCutoffRatio = PartsConstants::bbdCutoffRatio * bandwidthScale;

        if (stages != cachedStages) {
            cachedStages = stages;
            cachedStageScale = std::sqrt(PartsConstants::bbdStagesReference / std::max(1.0, static_cast<double>(stages)));
        }
        targetCutoffRatio *= cachedStageScale;
        targetCutoffRatio = std::clamp(targetCutoffRatio, 0.01, 0.45);

        double lpfAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * targetCutoffRatio);
        lpfAlpha = std::clamp(lpfAlpha, 0.001, 0.99);

        double lpfTweakFactor = 1.0 + PartsConstants::bbdSupplyBwFactor * supplyNorm;
        lpfAlpha *= lpfTweakFactor;
        lpfAlpha = std::clamp(lpfAlpha, 0.001, 0.99);

        for (size_t ch = 0; ch < samples.size(); ++ch)
        {
            double x = samples[ch];

            if (ch < bbdDAState.size()) {
                double lpfState = bbdDAState[ch];
                lpfState += lpfAlpha * (x - lpfState);
                bbdDAState[ch] = lpfState;
                x = lpfState;
            } else {
                x *= lpfAlpha;
            }

            double y = x;

            if (supplyVoltage > PartsConstants::V_supplyMinEnable)
            {
                double sat = supplyVoltage * PartsConstants::bbdSatHeadroomRatio;
                double softness = 1.0 + PartsConstants::bbdSatSoftness * (1.0 - supplyNorm);
                if (y > sat) y = sat + (y - sat) / (softness + std::fabs(y - sat));
                if (y < -sat) y = -sat + (y + sat) / (softness + std::fabs(y + sat));
            }

            samples[ch] = static_cast<float>(y);
        }
    }

    // Block processing API (high-level)
    void processBlock(float* const* io, int numChannels, int numSamples, double step, const Params& params) noexcept
    {
        std::vector<float> frame((size_t)numChannels);
        for (int i = 0; i < numSamples; ++i) {
            for (int ch = 0; ch < numChannels; ++ch)
                frame[(size_t)ch] = io[ch][i];
            process(frame, step, params.stages, params.supplyVoltage, params.enableAging, params.ageYears);
            for (int ch = 0; ch < numChannels; ++ch)
                io[ch][i] = frame[(size_t)ch];
        }
    }
    // Set inter-stage alpha value calculated from coupling capacitor / sample rate
    void setBaseAlphas(double a, double a2) noexcept { bbdStageA = a; bbdStageA2 = a2; }

    // Cap Grade ESR in use: low-grade capacitor with low leakage = no HF loss = brighter tone
    // calculated as sqrt(1/esr) from PluginProcessor (Stock:1.0, Film:1.41, AudioGrade:2.0)
    void setBandwidthScale(double s) noexcept { bandwidthScale = std::max(0.5, std::min(3.0, s)); }

private:
    double sampleRate;
    double bbdStageA;   // 1st stage low-pass alpha value (derived from coupling cap)
    double bbdStageA2;  // 2nd stage cascade alpha value (for more analog-like characteristics)

    // state retention per channel and per simulation stage
    std::vector<std::vector<float>> bbdStageStates; // [channel][stage]
    int lastSimulatedStages; // last simulated stage count (for resize determination)

    double bandwidthScale = 1.0; // bandwidth scale derived from CapGrade ESR

    // sqrt calculation cache (performance optimization: recalculates only when stages value changes)
    int cachedStages;
    double cachedStageScale;

    // dielectric absorption per-channel state
    std::vector<double> bbdDAState; // memory phenomenon from aging effects
    
    // LPF state for SIMD optimization (2-way parallel stereo processing) — optional
    // Uncomment this to use AudioCompat::SIMDRegister<float>
    // patina::compat::SIMDRegister<float> simdLpfState; // 4-float pack (2 channels used)
};
