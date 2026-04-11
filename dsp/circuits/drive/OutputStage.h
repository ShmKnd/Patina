#pragma once
#include <vector>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <random>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"

// Output module (extended with BBD-level analog behavior)
// - 3-pole cascade LPF + analog-style soft saturation
// - Output impedance model (load-dependent frequency response)
// - THD vs frequency response (distortion increases at HF)
// - DC offset drift (temperature-dependent random walk)
// - Output stage phase shift (phase delay accumulates per stage)
// - Short-circuit current limiting (overload input protection)
// - Slew rate limiting
class OutputStage
{
public:
    OutputStage() noexcept : sampleRate(PartsConstants::defaultSampleRate), alpha(1.0),
        rng(42), normalDist(0.0, 1.0) {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = sr;
        const size_t nCh = (size_t)std::max(1, numChannels);
        prevY1.assign(nCh, 0.0);
        prevY2.assign(nCh, 0.0);
        prevY3.assign(nCh, 0.0);
        slewState.assign(nCh, 0.0);
        offsetDrift.assign(nCh, 0.0);
        prevOut.assign(nCh, 0.0);
        loadCapState.assign(nCh, 0.0);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept { prepare(spec.numChannels, spec.sampleRate); }

    void reset() noexcept
    {
        std::fill(prevY1.begin(), prevY1.end(), 0.0);
        std::fill(prevY2.begin(), prevY2.end(), 0.0);
        std::fill(prevY3.begin(), prevY3.end(), 0.0);
        std::fill(slewState.begin(), slewState.end(), 0.0);
        std::fill(offsetDrift.begin(), offsetDrift.end(), 0.0);
        std::fill(prevOut.begin(), prevOut.end(), 0.0);
        std::fill(loadCapState.begin(), loadCapState.end(), 0.0);
    }

    void setCutoffHz(double fc) noexcept
    {
        if (fc <= 0.0 || sampleRate <= 1.0) { alpha.store(1.0); return; }
        double a = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * fc / sampleRate);
        if (!std::isfinite(a)) a = 1.0;
        a = std::clamp(a, 1e-12, 0.999999999);
        alpha.store(a);
    }

    // Output impedance setting (Ω) — affects interaction with load capacitance
    void setOutputImpedance(double ohms) noexcept { outputImpedance = std::max(1.0, ohms); }

    inline float process(int channel, float x, double supplyVoltage) noexcept
    {
        if (channel < 0 || (size_t)channel >= prevY1.size()) return x;
        const size_t ch = (size_t)channel;

        const double a = alpha.load();

        // --- 3-stage cascaded 1-pole low-pass filter (-18dB/oct) ---
        double y1 = a * static_cast<double>(x) + (1.0 - a) * prevY1[ch];
        prevY1[ch] = y1;
        double y2 = a * y1 + (1.0 - a) * prevY2[ch];
        prevY2[ch] = y2;
        double y3 = a * y2 + (1.0 - a) * prevY3[ch];
        prevY3[ch] = y3;

        double v = y3;

        // --- DC offset drift (input offset voltage variation with temperature)---
        {
            double noise = normalDist(rng) * kOffsetDriftRate;
            offsetDrift[ch] = offsetDrift[ch] * kOffsetDriftDecay + noise;
            v += offsetDrift[ch];
        }

        // --- THD vs frequency: distortion increases at HF (open-loop gain roll-off model)---
        {
            // Approximate instantaneous frequency estimation from signal rate of change
            double deltaV = v - prevOut[ch];
            double slewMag = std::abs(deltaV) * sampleRate;
            // Higher slew rate = higher frequency → inject small amount of 2nd order harmonics
            double thdInject = slewMag * kThdFreqCoeff;
            thdInject = std::min(thdInject, 0.01); // maximum 1% THD
            v += thdInject * v * v * 0.1; // 2nd-order harmonic ≈ v²
        }

        // --- Soft saturation linked to supply voltage ---
        if (std::isfinite(supplyVoltage) && supplyVoltage > 0.0)
        {
            // Short-circuit current limiting (current limit on overload input)
            const double maxCurrent = supplyVoltage / outputImpedance;
            const double maxV = maxCurrent * kLoadResistance;
            v = std::clamp(v, -maxV, maxV);

            const double satAmount = 1.0;
            double norm = v / (supplyVoltage * PartsConstants::bbdSatHeadroomRatio);
            double sat = std::tanh(norm * satAmount) * (supplyVoltage * PartsConstants::outputSatLimitRatio);
            const double blend = PartsConstants::outputSatBlend;
            // Asymmetric saturation (positive side has slightly more headroom — NPN/PNP push-pull imbalance)
            double asymV = (v >= 0.0) ? v : v * kNegAsymmetry;
            v = blend * asymV + (1.0 - blend) * sat;
            v = std::clamp(v, -supplyVoltage, supplyVoltage);
        }
        else
        {
            v = std::tanh(v);
        }

        // --- Slew rate limiting (TL072: 13V/µs → output stage is slightly slower)---
        {
            double maxSlew = kSlewRateVps / sampleRate;
            double diff = v - slewState[ch];
            if (diff > maxSlew) v = slewState[ch] + maxSlew;
            else if (diff < -maxSlew) v = slewState[ch] - maxSlew;
            slewState[ch] = v;
        }

        // --- LPF from output impedance × load capacitance (cable capacitance model)---
        {
            // Typical cable capacitance: 100pF/m × 3m = 300pF
            double RC = outputImpedance * kLoadCapacitance;
            double dt = 1.0 / sampleRate;
            double loadAlpha = dt / (RC + dt);
            loadAlpha = std::clamp(loadAlpha, 0.01, 1.0);
            loadCapState[ch] += loadAlpha * (v - loadCapState[ch]);
            v = loadCapState[ch];
        }

        prevOut[ch] = v;
        return static_cast<float>(v);
    }

    void processBlock(float* const* io, int numChannels, int numSamples, double supplyVoltage) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], supplyVoltage);
    }

private:
    // === Physical constants ===
    static constexpr double kSlewRateVps        = 8e6;     // 8V/µs (Output buffer stage)
    static constexpr double kOffsetDriftRate     = 5e-8;    // DC offset drift magnitude
    static constexpr double kOffsetDriftDecay    = 0.99998; // Drift decay rate
    static constexpr double kThdFreqCoeff        = 1e-8;    // THD frequency dependency coefficient
    static constexpr double kNegAsymmetry        = 0.96;    // Negative-side clipping asymmetry
    static constexpr double kLoadCapacitance     = 300e-12; // Load capacitance 300pF (cable)
    static constexpr double kLoadResistance      = 10000.0; // Load resistance 10kΩ (next stage input)

    double sampleRate;
    double outputImpedance = 75.0; // Output impedance (Ω)
    std::atomic<double> alpha;
    std::vector<double> prevY1, prevY2, prevY3;
    std::vector<double> slewState;
    std::vector<double> offsetDrift;
    std::vector<double> prevOut;
    std::vector<double> loadCapState;
    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;
};
