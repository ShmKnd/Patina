#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/OTA_Primitive.h"
#include "../../parts/DiodePrimitive.h"

// OTA Sallen-Key filter emulation
// - OTA (LM13700)-based Sallen-Key topology
// - 2-pole (-12dB/oct) HPF + LPF dual filter configuration
// - Aggressive resonance characteristic (capable of self-oscillation)
// - OTA input diode clipping (distortion on large signals)
// - Temperature-dependent gm drift
// - Component tolerance (left/right asymmetry from OTA mismatch)
//
// Circuit topology:
//   early type: OTA (LM13700) Sallen-Key
//   late type: OPA (LM358) Sallen-Key
//   this implementation models the OTA version (early type)
//
// 4-layer architecture:
//   Parts: OTA_Primitive (LM13700) × 2 + DiodePrimitive (input protection)
//   → Circuit: OtaSKFilter (Sallen-Key 2-pole)
//   → Effect: (used by upper-level modules)
//
// usage:
//   HPF → LPF cascade for classic OTA dual filter
//   can also be used as standalone HPF/LPF
class OtaSKFilter
{
public:
    enum class Mode : int
    {
        LowPass  = 0,
        HighPass = 1,
        BandPass = 2   // HPF → LPF internal cascade
    };

    struct Params
    {
        float cutoffHz    = 1000.0f;
        float resonance   = 0.0f;     // 0.0–1.0, self-oscillation at 1.0
        float drive       = 0.0f;     // OTA input overdrive 0.0–1.0
        float temperature = 25.0f;    // operating temperature (°C)
        int   mode        = 0;        // 0=LP, 1=HP, 2=BP (dual)
    };

    OtaSKFilter() noexcept
        : rng(41), normalDist(0.0, 1.0),
          ota1(OTA_Primitive(OTA_Primitive::LM13700(), 211)),
          ota2(OTA_Primitive(OTA_Primitive::LM13700(), 212)),
          inputDiode(DiodePrimitive(DiodePrimitive::OtaInputDiode()))
    {
    }

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        chState.resize(nCh);
        for (auto& st : chState) st = ChannelState{};
        updateCoefficients(1000.0f, 0.0f, 25.0f);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& st : chState) st = ChannelState{};
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

    void setMode(Mode m) noexcept { filterMode = m; }

    // single sample processing
    inline float process(int channel, float x, int modeOverride = -1) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);
        auto& st = chState[ch];
        Mode m = (modeOverride >= 0) ? (Mode)modeOverride : filterMode;

        // OTA input diode clipping (nonlinearity on large signals)
        double v0 = (double)x;
        if (driveAmount > 0.001)
        {
            double overdrive = 1.0 + driveAmount * 4.0;
            v0 = std::tanh(v0 * overdrive) / overdrive * (1.0 + driveAmount * 0.5);
        }

        // Thermal noise (using OTA primitive physical properties)
        double sigLevel = std::abs(v0);
        if (sigLevel > 1e-10)
            v0 += normalDist(rng) * ota1.getSpec().thermalNoise * std::min(1.0, sigLevel);

        float result = 0.0f;
        switch (m)
        {
            case Mode::LowPass:
                result = processLP(st.lp, v0);
                break;
            case Mode::HighPass:
                result = processHP(st.hp, v0);
                break;
            case Mode::BandPass:
            {
                // HPF → LPF cascade (OTA-SK dual filter)
                double hpOut = processHP(st.hp, v0);
                result = processLP(st.lp, hpOut);
                break;
            }
        }
        return result;
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.resonance, params.temperature);
        driveAmount = std::clamp((double)params.drive, 0.0, 1.0);
        filterMode = (Mode)std::clamp(params.mode, 0, 2);
        return process(channel, x);
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.resonance, params.temperature);
        driveAmount = std::clamp((double)params.drive, 0.0, 1.0);
        filterMode = (Mode)std::clamp(params.mode, 0, 2);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i]);
    }

private:
    // === Component layer (Parts) ===
    OTA_Primitive ota1;           // LM13700 OTA #1
    OTA_Primitive ota2;           // LM13700 OTA #2
    DiodePrimitive inputDiode;    // OTA-SK input protection diode

    // circuit constants (derived from OTA/diode specs)
    static constexpr double kMaxResonance    = 4.2;     // maximum k capable of reaching self-oscillation point

    // Sallen-Key 2-pole filter state
    struct FilterState
    {
        double s1 = 0.0;  // integrator 1
        double s2 = 0.0;  // integrator 2
    };

    struct ChannelState
    {
        FilterState lp;
        FilterState hp;
    };

    // --- LPF Sallen-Key (OTA) ---
    inline float processLP(FilterState& st, double x) noexcept
    {
        // OTA-SK LPF: OTA Sallen-Key with diode limiting in feedback
        double input = x - resoK * st.s2;

        // OTA integration: linear integration reflecting gm mismatch (large-signal saturation already handled by input drive stage)
        double v1 = st.s1 + gCoeff * ota1.getMismatch() * (input - st.s1);
        double v2 = st.s2 + gCoeff * ota2.getMismatch() * (v1 - st.s2);

        st.s1 = v1;
        st.s2 = v2;
        return (float)v2;
    }

    // --- HPF Sallen-Key (OTA) ---
    inline float processHP(FilterState& st, double x) noexcept
    {
        // OTA-SK HPF: input - 2×LP approximation
        double input = x - resoK * st.s2;
        double v1 = st.s1 + gCoeff * ota1.getMismatch() * (input - st.s1);
        double v2 = st.s2 + gCoeff * ota2.getMismatch() * (v1 - st.s2);
        st.s1 = v1;
        st.s2 = v2;

        // HP output: input - LP(2pole)
        return (float)(x - v2);
    }

    // OTA saturation function → delegated to component primitive
    inline double otaSaturate(double x) const noexcept
    {
        return ota1.saturate(x);
    }

    void updateCoefficients(float fc, float r, float temperature = 25.0f) noexcept
    {
        cutoffHz = std::clamp((double)fc, 20.0, sampleRate * 0.49);
        reso = std::clamp((double)r, 0.0, 1.0);
        lastTemp = temperature;

        // temperature-dependent gm scaling → obtained from OTA primitive
        double gmScale = ota1.gmScale(temperature);

        double effectiveFc = cutoffHz * gmScale;
        effectiveFc = std::clamp(effectiveFc, 20.0, sampleRate * 0.49);

        // bilinear transform coefficient
        gCoeff = std::tan(3.14159265358979323846 * effectiveFc / sampleRate);
        gCoeff = gCoeff / (1.0 + gCoeff);

        // resonance → feedback coefficient
        // OTA-SK is very aggressive: self-oscillation at reso=1.0
        resoK = reso * kMaxResonance;
    }

    double sampleRate = PartsConstants::defaultSampleRate;
    double cutoffHz   = 1000.0;
    double reso       = 0.0;
    float  lastTemp   = 25.0f;
    double gCoeff     = 0.0;
    double resoK      = 0.0;
    double driveAmount = 0.0;
    Mode   filterMode  = Mode::LowPass;

    std::vector<ChannelState> chState;
    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;
};
