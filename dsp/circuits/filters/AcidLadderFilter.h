#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../core/FastMath.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/BJT_Primitive.h"

// AcidLadderFilter
// - 3-pole (-18 dB/oct) BJT ladder LP with a compact resonant character
// - Input 1st-order RC HPF (~40 Hz): removes sub-bass for a lean low end
// - BP output mixed into LP: mix increases with resonance for an accented sweep
// - ZDF TPT formulation for accurate self-oscillation at reso=1.0
//
// Topology points:
//   The ladder is 3 BJT differential pairs in cascade (not diodes — distinct from K_LPF).
//   The characteristic response comes from:
//     1) The pre-HPF removing low-end for a focused midrange
//     2) BP bleed into LP output (resonance-dependent) for a swept formant character
//     3) BJT saturation nonlinearity on heavy drive → buzz at self-oscillation
//
// 4-layer architecture:
//   Parts: BJT_Primitive × 3
//   → Circuit: AcidLadderFilter (3-pole ladder + HPF input + BP mix)
class AcidLadderFilter
{
public:
    struct Params
    {
        float cutoffHz    = 1000.0f;
        float resonance   = 0.0f;
        float drive       = 0.0f;
        float temperature = 25.0f;
    };

    AcidLadderFilter() noexcept : rng (79)
    {
        for (int i = 0; i < 3; ++i)
            stageBjt[i] = BJT_Primitive (BJT_Primitive::Generic(), 179 + (unsigned)i * 53);
    }

    void prepare (int numChannels, double sr) noexcept
    {
        sampleRate = std::max (1.0, sr);
        const size_t nCh = (size_t)std::max (1, numChannels);
        stage.resize (nCh);
        for (auto& st : stage) st = ChannelState{};
        updateCoefficients (1000.0f, 0.0f, 25.0f);
    }

    void prepare (const patina::ProcessSpec& spec) noexcept
    {
        prepare (spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& st : stage) st = ChannelState{};
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

        // ── Input RC HPF (~40 Hz) ────────────────────────────────────────────
        // 1st-order HPF: y_hp = alpha * (y_prev + x - x_prev)
        // alpha = 1 / (1 + 2*pi*fc_hp/sr)  [bilinear approx]
        const double hpOut = hpAlpha * (st.hpY + (double)x - st.hpX);
        st.hpX = (double)x;
        st.hpY = hpOut;
        double input = hpOut;

        // Light BJT input drive, tuned for a compact low-headroom response.
        if (driveAmount > 0.001f)
        {
            const double driveGain = 1.0 + (double)driveAmount * 2.5;
            input = BJT_Primitive::saturate (input * driveGain);
        }

        // ── ZDF 3-pole ladder ────────────────────────────────────────────────
        // y2 * (1 + k*G^3) = G^3*input + G^3*s0/g + G^2*s1/g + G*s2/g
        const double gInv = (g > 1e-10) ? 1.0 / g : 0.0;
        const double G2 = G * G, G3 = G2 * G;
        const double S = G3 * st.s[0] * gInv + G2 * st.s[1] * gInv + G * st.s[2] * gInv;
        const double y2_zdf = (G3 * input + S) / (1.0 + resonanceScaled * G3);

        double x_in = input - resonanceScaled * y2_zdf;

        // 3-stage sequential
        for (int i = 0; i < 3; ++i)
        {
            const double m = stageBjt[i].getMismatch();
            const double y = G * (m * BJT_Primitive::saturate (x_in) + st.s[i] * gInv);

            // Thermal noise
            const double sigLevel = std::abs (y);
            if (sigLevel > 1e-10)
            {
                const double noise = fastNoise (rng) * stageBjt[i].getSpec().thermalNoise
                                     * std::min (1.0, sigLevel);
                x_in = FastMath::sanitize (y + noise);
            }
            else
            {
                x_in = 0.0;
            }

            // TPT state update
            st.s[i] = FastMath::sanitize (2.0 * y - st.s[i]);
        }

        // ── LP + BP mix ──────────────────────────────────────────────────────
        // LP is the final stage output.
        // BP estimate: stage 1 output approximates a bandpass tap.
        // Mix weight scales with resonance: adds the nasal "wah" sweep at high Q.
        const double lp = x_in;
        // BP tap: approximate as difference between adjacent states / G
        const double bpTap = st.s[1] - st.s[2];  // intermediate ladder node difference
        const double bpMix = reso * 0.28;  // resonance-dependent BP character
        const double out = lp + bpMix * bpTap;

        return (float)FastMath::sanitize (out);
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
    BJT_Primitive stageBjt[3];

    // HPF alpha coefficient (1st-order RC HPF pre-filter)
    static constexpr double kHpfHz    = 40.0;  // fixed input HPF

    void updateCoefficients (float fc, float r, float temperature = 25.0f) noexcept
    {
        freq = std::clamp ((double)fc, 20.0, sampleRate * 0.45);
        reso = std::clamp ((double)r, 0.0, 1.0);
        lastTemp = temperature;

        // BJT temperature-dependent fc
        const double tempScale = stageBjt[0].tempScale (temperature);
        const double effectiveFreq = std::clamp (freq * tempScale, 20.0, sampleRate * 0.45);

        // TPT g coefficient
        g = std::tan (3.14159265358979323846 * effectiveFreq / sampleRate);
        G = g / (1.0 + g);

        // HPF coefficient (bilinear 1st-order: alpha = 1/(1 + 2*pi*fc_hp/fs))
        // Approximation: alpha ≈ fs / (fs + 2*pi*fc_hp) (close enough for fc_hp << fs)
        hpAlpha = sampleRate / (sampleRate + 2.0 * 3.14159265358979323846 * kHpfHz);

        // k=4.0 reaches self-oscillation at reso=1.0.
        resonanceScaled = reso * 4.0;
    }

    struct ChannelState
    {
        double s[3]  = {};   // 3 TPT integrator states
        double hpX   = 0.0;  // HPF previous input
        double hpY   = 0.0;  // HPF previous output
    };

    double sampleRate      = PartsConstants::defaultSampleRate;
    double freq            = 1000.0;
    double reso            = 0.0;
    float  lastTemp        = 25.0f;
    double g               = 0.0;
    double G               = 0.0;
    double hpAlpha         = 0.0;
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
