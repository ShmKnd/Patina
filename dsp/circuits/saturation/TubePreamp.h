#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/TubeTriode.h"

// 12AX7 triode tube preamp emulation (extended with BBD-level analog behavior)
// - Plate characteristic curve (Koren simplified asymmetric waveshaper)
// - grid conduction (bias shift from grid current)
// - Miller capacitance (HF attenuation from plate-grid capacitance)
// - Plate impedance frequency dependence (rp decreases at HF → gain decrease)
// - Microphonics (noise injection from mechanical vibration)
// - Emission degradation (cathode lifespan model — gain/noise changes)
// - Supply voltage ripple (power supply hum — 50/60Hz)
//
// 4-layer architecture:
//   Parts: TubeTriode (12AX7) — Plate characteristics / Miller capacitance / grid conduction
//   → Circuit: TubePreamp (Bias + DC block + power supply)
class TubePreamp
{
public:
    struct Params
    {
        float drive = 0.5f;
        float bias = 0.5f;
        float outputLevel = 0.7f;
        bool enableGridConduction = true;
        float tubeAge = 0.0f;       // Tube aging 0.0 (new) – 1.0 (end of life)
        float supplyRipple = 0.0f;  // Power supply ripple amount 0.0–1.0
    };

    TubePreamp() noexcept
        : rng(77), normalDist(0.0, 1.0),
          tube(TubeTriode::T12AX7())
    {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        dcBlockState.assign(nCh, {0.0, 0.0});
        gridCapState.assign(nCh, 0.0);
        prevOutput.assign(nCh, 0.0);
        millerCapState.assign(nCh, 0.0);
        plateImpState.assign(nCh, 0.0);
        microphonicsPhase.assign(nCh, 0.0);
        ripplePhase = 0.0;

        const double fc = 20.0;
        dcBlockAlpha = 1.0 / (1.0 + 2.0 * 3.14159265358979323846 * fc / sampleRate);

        // Call TubeTriode prepare to calculate Miller capacitance / plate impedance coefficients
        tube.prepare(sampleRate);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& s : dcBlockState) { s.x = 0.0; s.y = 0.0; }
        std::fill(gridCapState.begin(), gridCapState.end(), 0.0);
        std::fill(prevOutput.begin(), prevOutput.end(), 0.0);
        std::fill(millerCapState.begin(), millerCapState.end(), 0.0);
        std::fill(plateImpState.begin(), plateImpState.end(), 0.0);
        std::fill(microphonicsPhase.begin(), microphonicsPhase.end(), 0.0);
        ripplePhase = 0.0;
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)dcBlockState.size() - 1);

        // Emission degradation model
        const double ageFactor = 1.0 - 0.4 * static_cast<double>(params.tubeAge); // Maximum 40% gain reduction
        const double ageNoiseFactor = 1.0 + 3.0 * static_cast<double>(params.tubeAge); // Noise increase

        // Drive gain (1x–16x) × emission degradation
        const double gain = (1.0 + params.drive * 15.0) * ageFactor;
        double v = (double)x * gain;

        // Bias offset (±0.8V — limited to safe range)
        const double biasV = (params.bias - 0.5) * 1.6;
        v += biasV;

        // --- Supply voltage ripple (50/60Hz hum injection)---
        if (params.supplyRipple > 0.001f)
        {
            const double rippleFreq = kMainsFreqHz;
            ripplePhase += rippleFreq / sampleRate;
            if (ripplePhase > 1.0) ripplePhase -= 1.0;
            double ripple = std::sin(ripplePhase * 2.0 * 3.14159265358979323846);
            // Full-wave rectified ripple (includes 2nd order harmonics)
            ripple += 0.3 * std::sin(ripplePhase * 4.0 * 3.14159265358979323846);
            v += ripple * kRippleAmplitude * static_cast<double>(params.supplyRipple);
        }

        // --- Miller capacitance (filtering using TubeTriode physical properties)---
        {
            millerCapState[ch] += tube.millerAlpha * (v - millerCapState[ch]);
            v = millerCapState[ch];
        }

        // grid conduction
        if (params.enableGridConduction)
        {
            const double gridAlpha = 0.001;
            if (v > 0.0)
            {
                gridCapState[ch] += gridAlpha * (v - gridCapState[ch]);
                v -= gridCapState[ch] * 0.3;
            }
            else
            {
                gridCapState[ch] *= (1.0 - gridAlpha * 0.1);
            }
        }

        // --- Microphonics (noise from mechanical vibration)---
        {
            // Random modulation near tube natural resonance frequency (100-300Hz)
            microphonicsPhase[ch] += kMicrophonicsFreq / sampleRate;
            if (microphonicsPhase[ch] > 1.0) microphonicsPhase[ch] -= 1.0;
            double mNoise = std::sin(microphonicsPhase[ch] * 2.0 * 3.14159265358979323846)
                          * normalDist(rng) * kMicrophonicsLevel * ageNoiseFactor;
            v += mNoise;
        }

        // 12AX7 plate characteristics (delegated to TubeTriode) + emission degradation
        v = tube.transferFunction(v, ageFactor);

        // Output level
        v *= params.outputLevel;

        // --- Plate impedance frequency dependence (TubeTriode physical properties)---
        {
            plateImpState[ch] += tube.plateAlpha * (v - plateImpState[ch]);
            v = v * 0.95 + plateImpState[ch] * 0.05;
        }

        // Cathode bypass capacitor effect
        const double cathodeLpfAlpha = 0.02;
        prevOutput[ch] += cathodeLpfAlpha * (v - prevOutput[ch]);
        v = v * 0.7 + prevOutput[ch] * 0.3;

        // Shot noise (cathode-derived — TubeTriode physical properties)
        v += normalDist(rng) * tube.getSpec().shotNoise * ageNoiseFactor;

        // DC block
        auto& dc = dcBlockState[ch];
        double dcOut = dcBlockAlpha * (dc.y + v - dc.x);
        dc.x = v;
        dc.y = dcOut;

        return (float)dcOut;
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

private:
    // === Component layer (Parts) ===
    TubeTriode tube;  // 12AX7

    // === circuit constants ===
    static constexpr double kMicrophonicsFreq  = 180.0;  // microphonics center frequency (Hz)
    static constexpr double kMicrophonicsLevel  = 2e-5;   // microphonics level
    static constexpr double kMainsFreqHz        = 50.0;   // Mains frequency (Hz) (Japan: 50/60)
    static constexpr double kRippleAmplitude    = 0.002;  // Ripple amplitude

    struct DcBlockState { double x = 0.0; double y = 0.0; };

    double sampleRate = PartsConstants::defaultSampleRate;
    double dcBlockAlpha = 0.999;
    double ripplePhase = 0.0;

    std::vector<DcBlockState> dcBlockState;
    std::vector<double> gridCapState;
    std::vector<double> prevOutput;
    std::vector<double> millerCapState;     // Miller capacitance LPF state
    std::vector<double> plateImpState;      // Plate impedance LPF state
    std::vector<double> microphonicsPhase;  // Microphonics phase

    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;
};
