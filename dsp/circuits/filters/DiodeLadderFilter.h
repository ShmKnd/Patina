#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../core/FastMath.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/DiodePrimitive.h"
#include "../../parts/RC_Element.h"

// Diode ladder filter emulation
// - 3-pole (-18dB/oct) diode ladder topology (distinct from transistor ladder)
// - Diode nonlinearity (different clipping characteristics per stage → "squelchy" sound)
// - Diode clipper in feedback path → distortion rides on resonance
// - Temperature-dependent diode Vf drift
// - Non-ideal inter-stage capacitance
//
// 4-layer architecture:
//   Parts: DiodePrimitive (diode ladder) × 3 + DiodePrimitive (FB)
//   → Circuit: DiodeLadderFilter (3-pole ladder)
//   → Effect: (used by upper-level modules)
class DiodeLadderFilter
{
public:
    struct Params
    {
        float cutoffHz    = 1000.0f;
        float resonance   = 0.0f;     // 0.0–1.0
        float drive       = 0.0f;     // input overdrive 0.0–1.0
        float temperature = 25.0f;
    };

    DiodeLadderFilter() noexcept
        : rng(67), normalDist(0.0, 1.0),
          fbDiode(DiodePrimitive(DiodePrimitive::Si1N4148()))
    {
        // 3-stage diodes: diode ladder low-Vf silicon (fixed variation per stage)
        for (int i = 0; i < 3; ++i)
            stageDiode[i] = DiodePrimitive(DiodePrimitive::LowVfSilicon());
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

    inline float process(int channel, float x, float driveAmount = 0.0f) noexcept
    {
        if (chState.empty()) return x;
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);
        auto& st = chState[ch];

        // input drive
        double drive = 1.0 + (double)driveAmount * 5.0;
        double v0 = (double)x * drive;

        // thermal noise
        double sigLevel = std::abs(v0);
        if (sigLevel > 1e-10)
            v0 += normalDist(rng) * kThermalNoise * std::min(1.0, sigLevel);

        // feedback (previous sample's final stage output, made nonlinear by diode primitive)
        double fb = fbDiode.feedbackClip(st.delay, lastTemp) * resonanceScaled;
        double input = v0 - fb;

        // 3-stage diode ladder — using DiodePrimitive for each stage
        for (int i = 0; i < 3; ++i)
        {
            double s = st.s[i];
            double gi = gCoeff;

            // diode nonlinearity: each stage's primitive (reflects component variation)
            double diff = stageDiode[i].saturate(input, lastTemp) - stageDiode[i].saturate(s, lastTemp);
            double y = s + gi * diff;
            st.s[i] = y;

            // minor crosstalk due to inter-stage capacitance
            if (i < 2)
            {
                double& cap = st.interCap[i];
                cap += kInterCapAlpha * (y - cap);
                y = y * 0.997 + cap * 0.003;
                st.s[i] = FastMath::sanitize(y);
            }

            input = y;
        }

        st.delay = st.s[2];

        // output is the final stage: -18dB/oct LP characteristic
        return (float)FastMath::sanitize(st.s[2]);
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.resonance, params.temperature);
        return process(channel, x, params.drive);
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.resonance, params.temperature);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params.drive);
    }

private:
    // === Component layer (Parts) ===
    DiodePrimitive stageDiode[3];   // 3-stage diodes (diode ladder low-Vf Si)
    DiodePrimitive fbDiode;         // feedback path diode

    // === Circuit constants (derived from Parts specs) ===
    static constexpr double kThermalNoise    = 2e-6;
    static constexpr double kTempCoeffVf     = -0.002;  // Vf temperature coefficient (V/°C)
    static constexpr double kInterCapAlpha   = 0.004;   // inter-stage coupling capacitance coefficient
    static constexpr double kMaxResonance    = 3.8;     // maximum feedback for 3-pole ladder

    struct ChannelState
    {
        double s[3]       = {};          // 3 stages' state
        double delay      = 0.0;         // feedback delay
        double interCap[2] = {};         // inter-stage capacitance
    };

    void updateCoefficients(float fc, float r, float temperature = 25.0f) noexcept
    {
        cutoffHz = std::clamp((double)fc, 20.0, sampleRate * 0.49);
        reso = std::clamp((double)r, 0.0, 1.0);
        lastTemp = temperature;

        // temperature dependence: diode Vf shift → minor cutoff variation
        // uses the effectiveVf of the Parts primitive
        double vf25 = stageDiode[0].getSpec().Vf_25C;
        double vfNow = stageDiode[0].effectiveVf(temperature);
        double vfScale = vfNow / vf25;
        vfScale = std::clamp(vfScale, 0.90, 1.10);

        double effectiveFc = cutoffHz * vfScale;
        effectiveFc = std::clamp(effectiveFc, 20.0, sampleRate * 0.49);

        gCoeff = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * effectiveFc / sampleRate);
        resonanceScaled = reso * kMaxResonance;
    }

    double sampleRate      = PartsConstants::defaultSampleRate;
    double cutoffHz        = 1000.0;
    double reso            = 0.0;
    float  lastTemp        = 25.0f;
    double gCoeff          = 0.0;
    double resonanceScaled = 0.0;

    std::vector<ChannelState> chState;
    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;
};
