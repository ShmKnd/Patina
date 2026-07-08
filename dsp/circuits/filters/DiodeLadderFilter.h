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
// - Diode nonlinearity (different clipping characteristics per stage)
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
        : rng(67),
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
            v0 += fastNoise(rng) * kThermalNoise * std::min(1.0, sigLevel);

        // ZDF 3-pole diode ladder: linear solve for y2 without unit delay
        // (diode feedbackClip is linear below Vf, so linear ZDF is exact at small signal;
        //  the clip engages naturally for large resonance amplitude)
        //   y_i = G * (sat(y_{i-1}) + s_i/g) [small signal: sat≈id]
        //   y2*(1 + k*G^3) = G^3*input + G^3*s0/g + G^2*s1/g + G*s2/g
        const double gInv = (gCoeff > 1e-10) ? 1.0 / gCoeff : 0.0;
        const double G2 = G * G, G3 = G2 * G;
        const double S = G3 * st.s[0] * gInv + G2 * st.s[1] * gInv + G * st.s[2] * gInv;
        const double y2_zdf = (G3 * v0 + S) / (1.0 + resonanceScaled * G3);

        // Apply diode clip to ZDF estimate → physical resonance amplitude limiting
        const double fb = fbDiode.feedbackClip(y2_zdf, lastTemp) * resonanceScaled;
        double x_in = v0 - fb;

        // 3-stage sequential processing with per-stage diode nonlinearity
        for (int i = 0; i < 3; ++i)
        {
            // TPT 1-pole integrator with diode saturation
            const double y = G * (stageDiode[i].saturate(x_in, lastTemp) + st.s[i] * gInv);

            // Inter-stage capacitance (minor crosstalk)
            double y_out = y;
            if (i < 2)
            {
                double& cap = st.interCap[i];
                cap += kInterCapAlpha * (y - cap);
                y_out = y * 0.997 + cap * 0.003;
            }

            // TPT state update: s_new = 2*y - s_old
            st.s[i] = FastMath::sanitize(2.0 * y - st.s[i]);

            x_in = y_out;
        }

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
        double s[3]       = {};          // 3 TPT integrator states
        double interCap[2] = {};         // inter-stage capacitance
    };

    void updateCoefficients(float fc, float r, float temperature = 25.0f) noexcept
    {
        cutoffHz = std::clamp((double)fc, 20.0, sampleRate * 0.45);
        reso = std::clamp((double)r, 0.0, 1.0);
        lastTemp = temperature;

        // temperature dependence: diode Vf shift → minor cutoff variation
        double vf25 = stageDiode[0].getSpec().Vf_25C;
        double vfNow = stageDiode[0].effectiveVf(temperature);
        double vfScale = std::clamp(vfNow / vf25, 0.90, 1.10);
        double effectiveFc = std::clamp(cutoffHz * vfScale, 20.0, sampleRate * 0.45);

        // TPT pre-warped g coefficient: enables ZDF self-oscillation at reso=1.0
        gCoeff = std::tan(3.14159265358979323846 * effectiveFc / sampleRate);
        G = gCoeff / (1.0 + gCoeff);

        resonanceScaled = reso * kMaxResonance;
    }

    double sampleRate      = PartsConstants::defaultSampleRate;
    double cutoffHz        = 1000.0;
    double reso            = 0.0;
    float  lastTemp        = 25.0f;
    double gCoeff          = 0.0;
    double G               = 0.0;   // TPT single-pole coefficient gCoeff/(1+gCoeff)
    double resonanceScaled = 0.0;

    std::vector<ChannelState> chState;
    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;  // unused placeholder
    static double fastNoise (std::minstd_rand& r) noexcept
    {
        constexpr double kInv = 1.0 / 2147483648.0;
        const double s = ((double)(int)r() + (double)(int)r()
                        + (double)(int)r() + (double)(int)r()) * kInv;
        return (s - 2.0) * 1.7320508;
    }
};
