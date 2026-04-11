#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../core/FastMath.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/OTA_Primitive.h"

// CEM3320 / OTA-SVF style state variable filter (extended with BBD-level analog behavior)
// - 2nd-order filter: LP, HP, BP, Notch simultaneous output
// - OTA nonlinearity (LM13700 gm curve → tanh saturation per integrator)
// - Temperature-dependent gm scaling
// - Output impedance load effect
// - Component tolerance (Rext / C variation)
// - Thermal noise (OTA input stage)
//
// 4-layer architecture:
//   Parts: OTA_Primitive (LM13700) — gm/noise/temperature characteristics
//   → Circuit: StateVariableFilter (SVF 2nd order)
class StateVariableFilter
{
public:
    enum class Type { LowPass = 0, HighPass = 1, BandPass = 2, Notch = 3 };

    struct Params
    {
        float cutoffHz = 1000.0f;
        float resonance = 0.5f;
        int type = 0;
        float temperature = 25.0f;  // operating temperature (°C)
    };

    StateVariableFilter() noexcept
        : rng(59), normalDist(0.0, 1.0),
          ota(OTA_Primitive(OTA_Primitive::LM13700(), 173))
    {
        // Component tolerance (fixed at manufacture: ±1% resistance + ±5% capacitor)
        std::minstd_rand initRng(173);
        std::normal_distribution<double> rTol(1.0, 0.005); // 0.5% precision resistance
        std::normal_distribution<double> cTol(1.0, 0.02);  // 2% capacitor
        rextTolerance = std::clamp(rTol(initRng), 0.95, 1.05);
        capTolerance  = std::clamp(cTol(initRng), 0.90, 1.10);
    }

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        ic1eq.assign(nCh, 0.0);
        ic2eq.assign(nCh, 0.0);
        otaSat1.assign(nCh, 0.0);
        otaSat2.assign(nCh, 0.0);
        updateCoefficients(1000.0f, 0.5f, 25.0f);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(ic1eq.begin(), ic1eq.end(), 0.0);
        std::fill(ic2eq.begin(), ic2eq.end(), 0.0);
        std::fill(otaSat1.begin(), otaSat1.end(), 0.0);
        std::fill(otaSat2.begin(), otaSat2.end(), 0.0);
    }

    void setCutoffHz(float hz) noexcept
    {
        cutoffHz = std::clamp((double)hz, 20.0, sampleRate * 0.49);
        updateCoefficients((float)cutoffHz, (float)reso, lastTemp);
    }

    void setResonance(float r) noexcept
    {
        reso = std::clamp((double)r, 0.0, 1.0);
        updateCoefficients((float)cutoffHz, (float)reso, lastTemp);
    }

    struct Output { float lp; float hp; float bp; float notch; };

    inline Output processAll(int channel, float x) noexcept
    {
        if (ic1eq.empty()) return {x, 0.0f, 0.0f, x};
        const size_t ch = (size_t)std::clamp(channel, 0, (int)ic1eq.size() - 1);
        double v0 = (double)x;

        // --- OTA input stage thermal noise (OTA primitive physical properties)---
        double sigMag = std::abs(v0);
        if (sigMag > 1e-10)
            v0 += normalDist(rng) * ota.getSpec().thermalNoise * std::min(1.0, sigMag);

        // --- standard SVF topology ---
        const double v3 = v0 - ic2eq[ch];
        const double v1 = a1 * ic1eq[ch] + a2 * v3;
        const double v2 = ic2eq[ch] + a2 * ic1eq[ch] + a3 * v3;
        ic1eq[ch] = FastMath::sanitize(2.0 * v1 - ic1eq[ch]);
        ic2eq[ch] = FastMath::sanitize(2.0 * v2 - ic2eq[ch]);

        // --- OTA nonlinearity (using OTA primitive's saturation function) ---
        double v1out = v1;
        double v2out = v2;
        if (std::abs(v1) > 0.1)
        {
            double otaDist = ota.saturate(v1) - v1;
            v1out += otaDist * kOtaNonlinMix;
        }
        if (std::abs(v2) > 0.1)
        {
            double otaDist = ota.saturate(v2) - v2;
            v2out += otaDist * kOtaNonlinMix;
        }

        // --- Output impedance load effect ---
        const double loadFactor = kOutputLoadFactor;

        Output out;
        out.lp    = (float)(v2out * loadFactor);
        out.bp    = (float)(v1out * loadFactor);
        out.hp    = (float)((v0 - effectiveK * v1out - v2out) * loadFactor);
        out.notch = out.lp + out.hp;
        return out;
    }

    inline float process(int channel, float x, int typeIndex = 0) noexcept
    {
        auto out = processAll(channel, x);
        switch (typeIndex)
        {
            case 1:  return out.hp;
            case 2:  return out.bp;
            case 3:  return out.notch;
            default: return out.lp;
        }
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.resonance, params.temperature);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params.type);
    }

private:
    // === Component layer (Parts) ===
    OTA_Primitive ota;  // LM13700 OTA

    // === circuit constants ===
    static constexpr double kOtaNonlinMix    = 0.08;    // nonlinear blend ratio
    static constexpr double kOutputLoadFactor= 0.9995;  // output load loss

    void updateCoefficients(float fc, float r, float temperature = 25.0f) noexcept
    {
        cutoffHz = std::clamp((double)fc, 20.0, sampleRate * 0.49);
        reso = std::clamp((double)r, 0.0, 1.0);
        lastTemp = temperature;

        // temperature-dependent gm scaling (delegated to OTA primitive)
        double gmScale = ota.gmScale(temperature);

        // apply component tolerance
        double effectiveFc = cutoffHz * rextTolerance * capTolerance * gmScale;
        effectiveFc = std::clamp(effectiveFc, 20.0, sampleRate * 0.49);

        const double w = std::tan(3.14159265358979323846 * effectiveFc / sampleRate);
        const double Q = 0.5 + reso * 19.5;
        effectiveK = 1.0 / Q;
        a1 = 1.0 / (1.0 + effectiveK * w + w * w);
        a2 = w * a1;
        a3 = w * a2;
    }

    double sampleRate = PartsConstants::defaultSampleRate;
    double cutoffHz = 1000.0;
    double reso = 0.5;
    float lastTemp = 25.0f;
    double effectiveK = 1.0;
    double a1 = 0.0, a2 = 0.0, a3 = 0.0;

    // component tolerance (fixed variation)
    double rextTolerance = 1.0;
    double capTolerance = 1.0;

    std::vector<double> ic1eq;
    std::vector<double> ic2eq;
    std::vector<double> otaSat1;  // OTA saturation state
    std::vector<double> otaSat2;

    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;
};
