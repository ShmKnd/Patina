#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/TransformerPrimitive.h"

// British console-style audio transformer model (extended with BBD-level analog behavior)
// - Magnetic saturation (hysteresis — simplified Jiles-Atherton)
// - LF boost (transformer winding coupling characteristic ~80 Hz)
// - HF rolloff (leakage inductance ~18kHz)
// - Frequency-dependent core loss (iron loss = hysteresis loss + eddy current loss)
// - DC bias effect (asymmetric shift of magnetization curve)
// - Slight HF attenuation from winding resistance
// - Interleaved winding capacitance (resonance peak)
// - Thermal drift (core permeability change with temperature)
//
// 4-layer architecture:
//   Parts: TransformerPrimitive (BritishConsole) — saturation/core loss/resonance/temperature
//   → Circuit: TransformerModel (LF boost + HF rolloff + DC bias)
class TransformerModel
{
public:
    struct Params
    {
        float driveDb = 0.0f;
        float saturation = 0.5f;
        bool enableLfBoost = true;
        bool enableHfRolloff = true;
        float dcBias = 0.0f;        // DC bias amount -1.0–+1.0
        float temperature = 40.0f;  // Core temperature (°C) — Higher during operation
    };

    TransformerModel() noexcept
        : rng(113), normalDist(0.0, 1.0),
          xfmr(TransformerPrimitive::BritishConsole())
    {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        lfBoostState.assign(nCh, 0.0);

        lfAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * 80.0 / sampleRate);
        hfAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * 18000.0 / sampleRate);

        // TransformerPrimitive prepare() calculates winding resonance coefficients
        xfmr.prepare(sampleRate);

        // Resonance coefficients for saturation
        const auto& spec = xfmr.getSpec();
        double resonanceFreq = 1.0 / (2.0 * 3.14159265358979323846
                               * std::sqrt(spec.leakageL * spec.windingCap));
        double wrAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * resonanceFreq / sampleRate);
        resonanceAlpha = std::clamp(wrAlpha, 0.001, 1.0);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(lfBoostState.begin(), lfBoostState.end(), 0.0);
        hystState = 0.0;
        coreLossAccum = 0.0;
        prevSample = 0.0;
        resonanceState = { 0.0, 0.0 };
        hfState = 0.0;
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)lfBoostState.size() - 1);

        const double driveGain = std::pow(10.0, params.driveDb / 20.0);
        double v = (double)x * driveGain;

        // --- Thermal drift (delegated to TransformerPrimitive)---
        double muScale = xfmr.muScale(params.temperature);

        // --- DC bias effect (operating point shift of magnetization curve → increased asymmetric distortion)---
        double dcOffset = (double)params.dcBias * kDcBiasScale;
        v += dcOffset;

        // LF boost (primitive material properties + external state)
        if (params.enableLfBoost)
        {
            double& lfs = lfBoostState[ch];
            lfs += lfAlpha * (v - lfs);
            v += (lfs - v) * 0.18;
        }

        // --- Magnetic saturation (hysteresis)---
        {
            const auto& spec = xfmr.getSpec();
            double satGain = (1.0 + (double)params.saturation * 2.0) * muScale;
            double input = v * satGain;
            double delta = input - hystState;
            double alpha = 0.95 - (double)params.saturation * 0.3;
            if (std::abs(delta) > 0.01)
            {
                double rate = (delta > 0.0) ? (1.0 - alpha * 0.9) : (1.0 - alpha * 1.1);
                hystState += delta * std::clamp(rate, 0.01, 1.0);
            }
            v = std::tanh(hystState * (0.8 + (double)params.saturation * 0.4))
                / (0.8 + (double)params.saturation * 0.4) * satGain;
        }

        // --- Frequency-dependent core loss ---
        {
            double deltaV = v - prevSample;
            double slewRate = std::abs(deltaV) * sampleRate;
            double loss = slewRate * slewRate * xfmr.getSpec().coreLossCoeff;
            loss = std::min(loss, 0.02);
            coreLossAccum = coreLossAccum * 0.999 + loss * 0.001;
            prevSample = v;
            v = v * (1.0 - coreLossAccum);
        }

        // --- Interleaved winding capacitance resonance ---
        {
            double resQ = 3.0;
            double hp = v - resonanceState.s2 - resonanceState.s1 / resQ;
            resonanceState.s1 += resonanceAlpha * hp;
            resonanceState.s2 += resonanceAlpha * resonanceState.s1;
            v = v + resonanceState.s1 * xfmr.getSpec().resonanceGain;
        }

        // HF rolloff
        if (params.enableHfRolloff)
        {
            hfState += hfAlpha * (v - hfState);
            v = hfState;
        }

        // DC bias removal
        v -= dcOffset;

        // Thermal noise (TransformerPrimitive physical properties)
        v += normalDist(rng) * xfmr.getSpec().noiseLevel;

        v /= driveGain;

        return (float)v;
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

private:
    // === Component layer (Parts) ===
    TransformerPrimitive xfmr;  // British console

    // === circuit constants ===
    static constexpr double kDcBiasScale = 0.05;  // DC bias scale

    struct ResonanceState { double s1 = 0.0; double s2 = 0.0; };

    double sampleRate = PartsConstants::defaultSampleRate;
    double lfAlpha = 0.0;
    double hfAlpha = 0.0;
    double resonanceAlpha = 0.5;

    // Saturation processing state (single channel)
    double hystState = 0.0;
    double coreLossAccum = 0.0;
    double prevSample = 0.0;
    ResonanceState resonanceState;
    double hfState = 0.0;

    // Per-channel LF boost state
    std::vector<double> lfBoostState;

    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;
};
