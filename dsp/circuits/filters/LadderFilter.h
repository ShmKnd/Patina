#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../core/FastMath.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/BJT_Primitive.h"

// Transistor ladder filter emulation (extended with BBD-level analog behavior)
// - 4-pole (-24dB/oct) Huovilainen / Välimäki improved model
// - Transistor pair mismatch (minor gm variation per stage)
// - Temperature-dependent cutoff frequency tracking (BJT Vbe ≈ -2mV/°C)
// - Phase distortion from pole interaction (inter-stage coupling capacitance)
// - Drive-dependent resonance damping (Q decreases with high-level input)
// - thermal noise injection (transistor thermal noise per stage)
// - slight cutoff fluctuation from supply voltage variation
//
// 4-layer architecture:
//   Parts: BJT_Primitive × 4 (differential pairs)
//   → Circuit: LadderFilter (4-pole cascade)
class LadderFilter
{
public:
    struct Params
    {
        float cutoffHz = 1000.0f;
        float resonance = 0.0f;
        float drive = 0.0f;
        float temperature = 25.0f;   // operating temperature (°C)
    };

    LadderFilter() noexcept : rng(31)
    {
        // 4-stage BJT differential pairs (variation via different seeds per stage)
        for (int i = 0; i < 4; ++i)
            stageBjt[i] = BJT_Primitive(BJT_Primitive::Generic(), 123 + (unsigned)i * 37);
    }

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        stage.resize(nCh);
        for (auto& ch : stage)
        {
            std::fill(std::begin(ch.s), std::end(ch.s), 0.0);
            std::fill(std::begin(ch.interStageCap), std::end(ch.interStageCap), 0.0);
        }
        updateCoefficients(1000.0f, 0.0f, 25.0f);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& ch : stage)
        {
            std::fill(std::begin(ch.s), std::end(ch.s), 0.0);
            std::fill(std::begin(ch.interStageCap), std::end(ch.interStageCap), 0.0);
        }
    }

    void setCutoffHz(float hz) noexcept
    {
        freq = std::clamp((double)hz, 20.0, sampleRate * 0.49);
        updateCoefficients((float)freq, (float)reso, lastTemp);
    }

    void setResonance(float r) noexcept
    {
        reso = std::clamp((double)r, 0.0, 1.0);
        updateCoefficients((float)freq, (float)reso, lastTemp);
    }

    inline float process(int channel, float x, float driveAmount = 0.0f) noexcept
    {
        if (stage.empty()) return x;
        const size_t ch = (size_t)std::clamp(channel, 0, (int)stage.size() - 1);
        auto& st = stage[ch];

        const double driveGain = 1.0 + (double)driveAmount * 3.0;
        double input = BJT_Primitive::saturate((double)x * driveGain);

        // ZDF 4-pole transistor ladder.
        // Solve for y3 without unit delay by substituting stage equations:
        //   y_i = G * (m_i * tanh(y_{i-1}) + s_i / g)  [linear approx: tanh ≈ id]
        //   y3 * (1 + k*G^4) = G^4*input + G^4*s0/g + G^3*s1/g + G^2*s2/g + G*s3/g
        const double gInv = (g > 1e-10) ? 1.0 / g : 0.0;
        const double G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const double S = G4 * st.s[0] * gInv + G3 * st.s[1] * gInv
                       + G2 * st.s[2] * gInv + G  * st.s[3] * gInv;
        const double y3_zdf = (G4 * input + S) / (1.0 + resonanceScaled * G4);

        // Effective input after ZDF resonance feedback
        double x_in = input - resonanceScaled * y3_zdf;

        // 4-stage sequential processing with BJT nonlinearity and mismatch
        for (int i = 0; i < 4; ++i)
        {
            const double m = stageBjt[i].getMismatch();
            // TPT 1-pole integrator with BJT tanh nonlinearity
            const double y = G * (m * BJT_Primitive::saturate(x_in) + st.s[i] * gInv);

            // Inter-stage capacitive coupling (minor HF rolloff, applied to signal passed forward)
            double y_out = y;
            if (i < 3)
                y_out = stageBjt[i].interStageCoupling(y, st.interStageCap[i]);

            // Thermal noise (BJT Johnson-Nyquist)
            double signalLevel = std::abs(y_out);
            if (signalLevel > 1e-10)
                y_out += fastNoise(rng) * stageBjt[i].getSpec().thermalNoise * std::min(1.0, signalLevel);

            // TPT state update: s_new = 2*y - s_old
            st.s[i] = FastMath::sanitize(2.0 * y - st.s[i]);

            x_in = y_out;
        }

        return (float)FastMath::sanitize(x_in);
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
    BJT_Primitive stageBjt[4];  // 4-stage BJT differential pairs

    // === Circuit constants (derived from BJT spec) ===
    static constexpr double kSupplyVoltageNominal   = 12.0;   // Nominal supply voltage (V)

    void updateCoefficients(float fc, float r, float temperature = 25.0f) noexcept
    {
        freq = std::clamp((double)fc, 20.0, sampleRate * 0.45);
        reso = std::clamp((double)r, 0.0, 1.0);
        lastTemp = temperature;

        // temperature-dependent cutoff: uses BJT_Primitive's temperature scale
        double tempScale = stageBjt[0].tempScale(temperature);
        double effectiveFreq = freq * tempScale;

        // Slight supply voltage fluctuation (±0.1% random)
        double supplyJitter = 1.0 + fastNoise(rng) * 0.001;
        effectiveFreq = std::clamp(effectiveFreq * supplyJitter, 20.0, sampleRate * 0.45);

        // TPT pre-warped g coefficient: enables ZDF self-oscillation at reso=1.0
        // (replaces forward-Euler 1-exp formula which could never reach unit circle)
        g = std::tan(3.14159265358979323846 * effectiveFreq / sampleRate);
        G = g / (1.0 + g);

        resonanceScaled = reso * 4.0;
    }

    struct ChannelState
    {
        double s[4] = {};
        double interStageCap[3] = {}; // inter-stage capacitance state
    };

    double sampleRate = PartsConstants::defaultSampleRate;
    double freq = 1000.0;
    double reso = 0.0;
    float lastTemp = 25.0f;
    double g = 0.0;
    double G = 0.0;   // TPT single-pole coefficient g/(1+g)
    double resonanceScaled = 0.0;
    std::vector<ChannelState> stage;
    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;  // unused placeholder for linker compat
    static double fastNoise (std::minstd_rand& r) noexcept
    {
        constexpr double kInv = 1.0 / 2147483648.0;
        const double s = ((double)(int)r() + (double)(int)r()
                        + (double)(int)r() + (double)(int)r()) * kInv;
        return (s - 2.0) * 1.7320508;
    }
};
