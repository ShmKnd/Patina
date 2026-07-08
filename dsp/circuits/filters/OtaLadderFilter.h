#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../core/FastMath.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/OTA_Primitive.h"

// OTA 4-pole ladder filter
// - 4-pole (-24 dB/oct) LP using OTA as integrating elements in a ladder topology
// - ZDF TPT formulation (Zavalishin §3.10 extended to 4-pole)
// - OTA tanh saturation is softer/silkier than BJT (different curve shape)
// - Temperature-dependent gm drift (-0.3%/°C, OTA characteristic)
// - Per-stage gm mismatch (component variation ±1.5%)
// - Thermal noise (OTA Johnson-Nyquist)
//
// Character vs LadderFilter (BJT):
//   BJT saturate = std::tanh(x)          — hard, immediate saturation
//   OTA saturate = tanh(x*0.4/Vsat)*Vsat — softer onset, more headroom
//
// 4-layer architecture:
//   Parts: OTA_Primitive (LM13700) × 4
//   → Circuit: OtaLadderFilter (4-pole OTA ladder)
class OtaLadderFilter
{
public:
    struct Params
    {
        float cutoffHz    = 1000.0f;
        float resonance   = 0.0f;
        float drive       = 0.0f;
        float temperature = 25.0f;
    };

    OtaLadderFilter() noexcept : rng (53)
    {
        // 4-stage OTA differential pairs (seed offsets give per-stage mismatch variation)
        for (int i = 0; i < 4; ++i)
            stageOta[i] = OTA_Primitive (OTA_Primitive::LM13700(), 317 + (unsigned)i * 43);
    }

    void prepare (int numChannels, double sr) noexcept
    {
        sampleRate = std::max (1.0, sr);
        const size_t nCh = (size_t)std::max (1, numChannels);
        stage.resize (nCh);
        for (auto& ch : stage) ch = ChannelState{};
        updateCoefficients (1000.0f, 0.0f, 25.0f);
    }

    void prepare (const patina::ProcessSpec& spec) noexcept
    {
        prepare (spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& ch : stage) ch = ChannelState{};
    }

    void setCutoffHz (float hz) noexcept
    {
        freq = std::clamp ((double)hz, 20.0, sampleRate * 0.45);
        updateCoefficients ((float)freq, (float)reso, lastTemp);
    }

    void setResonance (float r) noexcept
    {
        reso = std::clamp ((double)r, 0.0, 1.0);
        updateCoefficients ((float)freq, (float)reso, lastTemp);
    }

    inline float process (int channel, float x, float driveAmount = 0.0f) noexcept
    {
        if (stage.empty()) return x;
        const size_t ch = (size_t)std::clamp (channel, 0, (int)stage.size() - 1);
        auto& st = stage[ch];

        // Light OTA input drive (softer than BJT — OTA is already limited by Vsat internally)
        const double driveGain = 1.0 + (double)driveAmount * 2.0;
        double input = stageOta[0].saturate ((double)x * driveGain);

        // ZDF 4-pole OTA ladder
        // y_i = G * (m_i * sat(y_{i-1}) + s_i/g)   [small signal: sat ≈ id]
        // y3 * (1 + k*G^4) = G^4*input + G^4*s0/g + G^3*s1/g + G^2*s2/g + G*s3/g
        const double gInv = (g > 1e-10) ? 1.0 / g : 0.0;
        const double G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const double S = G4 * st.s[0] * gInv + G3 * st.s[1] * gInv
                       + G2 * st.s[2] * gInv + G  * st.s[3] * gInv;
        const double y3_zdf = (G4 * input + S) / (1.0 + resonanceScaled * G4);

        double x_in = input - resonanceScaled * y3_zdf;

        for (int i = 0; i < 4; ++i)
        {
            const double m = stageOta[i].getMismatch();
            // TPT 1-pole integrator with OTA tanh nonlinearity (softer than BJT)
            const double y = G * (m * stageOta[i].saturate (x_in) + st.s[i] * gInv);

            // Thermal noise (OTA is slightly noisier than BJT due to shot noise)
            double y_out = y;
            const double sigLevel = std::abs (y_out);
            if (sigLevel > 1e-10)
                y_out += fastNoise (rng) * stageOta[i].getSpec().thermalNoise * std::min (1.0, sigLevel);

            // TPT state update: s_new = 2*y - s_old
            st.s[i] = FastMath::sanitize (2.0 * y - st.s[i]);

            x_in = y_out;
        }

        return (float)FastMath::sanitize (x_in);
    }

    inline float process (int channel, float x, const Params& params) noexcept
    {
        updateCoefficients (params.cutoffHz, params.resonance, params.temperature);
        return process (channel, x, params.drive);
    }

    void processBlock (float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        updateCoefficients (params.cutoffHz, params.resonance, params.temperature);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process (ch, io[ch][i], params.drive);
    }

private:
    OTA_Primitive stageOta[4];

    void updateCoefficients (float fc, float r, float temperature = 25.0f) noexcept
    {
        freq = std::clamp ((double)fc, 20.0, sampleRate * 0.45);
        reso = std::clamp ((double)r, 0.0, 1.0);
        lastTemp = temperature;

        // OTA gm-based temperature dependence
        const double gmScale = stageOta[0].gmScale (temperature);
        const double effectiveFreq = std::clamp (freq * gmScale, 20.0, sampleRate * 0.45);

        // TPT pre-warped g coefficient
        g = std::tan (3.14159265358979323846 * effectiveFreq / sampleRate);
        G = g / (1.0 + g);

        // The softer OTA curve keeps self-oscillation gentler than the BJT ladder.
        // k_max = 4.0 reaches oscillation without needing a delayed-feedback shortcut.
        resonanceScaled = reso * 4.0;
    }

    struct ChannelState
    {
        double s[4] = {};
    };

    double sampleRate      = PartsConstants::defaultSampleRate;
    double freq            = 1000.0;
    double reso            = 0.0;
    float  lastTemp        = 25.0f;
    double g               = 0.0;
    double G               = 0.0;
    double resonanceScaled = 0.0;

    std::vector<ChannelState> stage;
    std::minstd_rand rng;

    static double fastNoise (std::minstd_rand& r) noexcept
    {
        constexpr double kInv = 1.0 / 2147483648.0;
        const double s = ((double)(int)r() + (double)(int)r()
                        + (double)(int)r() + (double)(int)r()) * kInv;
        return (s - 2.0) * 1.7320508;
    }
};
