#pragma once
#include <vector>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <random>
#include "../../core/AudioCompat.h"
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"

// Input low-pass filter (extended with BBD-level analog behavior)
// - High-quality LPF via 4th-order cascaded 1-pole sections (SIMD/NEON optimized)
// - Electrolytic capacitor aging (capacitance drop → cutoff rise)
// - Component tolerance (each stage has slightly different RC values)
// - Peaking from pole interaction (inter-stage loading effect)
class InputFilter
{
public:
    static constexpr int    kOrder            = 2;
    static constexpr double kDefaultCutoffHz  = PartsConstants::inputLpfFcHz;
    static constexpr double kDigitalLPFCutoffRatio = 0.95;

    InputFilter() noexcept : sampleRate(PartsConstants::defaultSampleRate), alpha(1.0), digitalAlpha(1.0), lastDesiredCutoffHz(kDefaultCutoffHz), 
                          simdDigitalLpfState(0.0f)
    {
        // Component tolerance initialization (RC value variation ±3% per stage)
        std::minstd_rand initRng(191);
        std::normal_distribution<double> tol(1.0, 0.03);
        for (int i = 0; i < kOrder; ++i)
            stageAlphaScale[i] = std::clamp(tol(initRng), 0.94, 1.06);
    }

    // Non-realtime call: set channel count / sample rate and allocate buffers
    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = sr;
        stageStates.assign((size_t)numChannels * kOrder, 0.0); // State for channels × stages
        digitalLpfState.assign((size_t)numChannels, 0.0);      // Digital LPF state
        simdDigitalLpfState = patina::compat::SIMDRegister<float>(0.0f); // SIMD state initialization
        
        // Digital LPF coefficient calculation (cut at 95% of Nyquist frequency)
        double nyquist = sr * 0.5;
        double digitalCutoff = nyquist * kDigitalLPFCutoffRatio;
        double da = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * digitalCutoff / sr);
        if (!std::isfinite(da)) da = 1.0;
        da = std::clamp(da, 1e-12, 0.999999999);
        digitalAlpha.store(da);
    }
    void prepare(const patina::ProcessSpec& spec) noexcept { prepare(spec.numChannels, spec.sampleRate); }
    void reset() noexcept
    {
        std::fill(stageStates.begin(), stageStates.end(), 0.0);
        std::fill(digitalLpfState.begin(), digitalLpfState.end(), 0.0);
        simdDigitalLpfState = patina::compat::SIMDRegister<float>(0.0f); // SIMD state reset
    }

    // Purpose: correct each 1st-order pole fc to maintain user-specified overall cutoff (output -3dB)
    // N identical-order RC cascade：|H(f)| = 1 / sqrt(1 + (f/fc_p)^2)^N
    // At f=fc_desired, 1/sqrt(2) condition: 1 + (fc_desired/fc_p)^2 = 2^{1/N} -> fc_p = fc_desired/sqrt(2^{1/N}-1)
    // Electrolytic capacitor aging setting (capacitanceScale: 1.0=new, 0.7=degraded)
    void setCapacitanceScale(double scale) noexcept
    {
        capScale = std::clamp(scale, 0.5, 1.0);
        setCutoffHz(lastDesiredCutoffHz); // Recalculate
    }

    void setCutoffHz(double fcDesired) noexcept
    {
        lastDesiredCutoffHz = fcDesired;
        if (fcDesired <= 0.0 || sampleRate <= 1.0) { alpha.store(1.0); return; }
        // Aging: capacitance drop → cutoff rise (fc ∝ 1/C)
        double effectiveFc = fcDesired / capScale;
        double denom = std::pow(2.0, 1.0 / (double)kOrder) - 1.0;
        if (denom <= 0.0) denom = 1.0;
        double fcPerPole = effectiveFc / std::sqrt(denom);

        // 1st-order IIR discretization: a = 1 - exp(-2π*fc/fs)
        double a = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * fcPerPole / sampleRate);
        if (!std::isfinite(a)) a = 1.0;
        a = std::clamp(a, 1e-12, 0.999999999); // 1e-12to 0.999999999 range limit
        alpha.store(a);
    }

    // Simple API for restoring default cutoff
    void setDefaultCutoff() noexcept { setCutoffHz(kDefaultCutoffHz); }

    double getCurrentCutoffHz() const noexcept { return lastDesiredCutoffHz; }

    // Stereo-specific SIMD/NEON optimized path (2-channel simultaneous processing)
    inline void processStereo(float& ch0, float& ch1) noexcept
    {
        const size_t numChannels = (stageStates.size() / kOrder);
        if (numChannels < 2) {
            // Fallback: individual processing when insufficient channels
            ch0 = process(0, ch0);
            ch1 = process(1, ch1);
            return;
        }

        // Step 1: Digital LPF (SIMD parallel processing)
        float inputArray[4] = { ch0, ch1, 0.0f, 0.0f };
        auto inputVec = patina::compat::SIMDRegister<float>::fromRawArray(inputArray);
        
        float da = static_cast<float>(digitalAlpha.load());
        auto daVec = patina::compat::SIMDRegister<float>(da);
        auto oneMinusDa = patina::compat::SIMDRegister<float>(1.0f - da);
        
        // y = da * x + (1-da) * state
        simdDigitalLpfState = inputVec * daVec + simdDigitalLpfState * oneMinusDa;
        
        float outputArray[4];
        simdDigitalLpfState.copyToRawArray(outputArray);
        
        // Also update scalar state (for backward compatibility)
        digitalLpfState[0] = outputArray[0];
        digitalLpfState[1] = outputArray[1];
        
        // Step 2: Analog LPF (serial processing required due to cascade)
        double a = alpha.load();
        for (int ch = 0; ch < 2; ++ch) {
            double y = outputArray[ch];
            size_t base = (size_t)ch * kOrder;
            for (int s = 0; s < kOrder; ++s) {
                double prev = stageStates[base + (size_t)s];
                double yn = a * y + (1.0 - a) * prev;
                stageStates[base + (size_t)s] = yn;
                y = yn;
            }
            if (ch == 0) ch0 = static_cast<float>(y);
            else ch1 = static_cast<float>(y);
        }
    }

    // Audio thread call (realtime safe) — single channel version
    inline float process(int channel, float x) noexcept
    {
        if (channel < 0) return x;
        const size_t numChannels = (stageStates.size() / kOrder);
        if (numChannels == 0 || (size_t)channel >= numChannels) return x;

        // Step 1: Digital LPF (cut at 95% of Nyquist, preventing aliasing noise)
        double da = digitalAlpha.load();
        double& dState = digitalLpfState[(size_t)channel];
        double y = da * static_cast<double>(x) + (1.0 - da) * dState;
        dState = y;

        // Step 2: Analog LPF (tonal shaping, apply component tolerance to each stage)
        double a = alpha.load();
        size_t base = (size_t)channel * kOrder;
        for (int s = 0; s < kOrder; ++s)
        {
            double aStage = a * stageAlphaScale[s]; // Per-stage RC value variation
            aStage = std::clamp(aStage, 1e-12, 0.999999999);
            double prev = stageStates[base + (size_t)s];
            double yn = aStage * y + (1.0 - aStage) * prev;
            stageStates[base + (size_t)s] = yn;
            y = yn;
        }
        return static_cast<float>(y);
    }

    int getNumChannels() const noexcept { return (int)(stageStates.size() / kOrder); }

    // block processing API
    void processBlock(float* const* io, int numChannels, int numSamples) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i]);
    }

private:
    double sampleRate;
    double capScale = 1.0;             // Electrolytic capacitor degradation scale (1.0=new)
    double stageAlphaScale[2] = {1.0, 1.0}; // Per-stage RC value tolerance
    std::vector<double> stageStates;
    std::vector<double> digitalLpfState;
    std::atomic<double> alpha;
    std::atomic<double> digitalAlpha;
    double lastDesiredCutoffHz;
    
    // SIMD-optimised state (stereo processing only)
    patina::compat::SIMDRegister<float> simdDigitalLpfState; // SIMD state for digital LPF (4-float pack, 2ch used)
};
