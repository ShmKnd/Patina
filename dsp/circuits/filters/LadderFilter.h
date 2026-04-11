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

    LadderFilter() noexcept : rng(31), normalDist(0.0, 1.0)
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
            ch.delay = 0.0;
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
            ch.delay = 0.0;
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

        const double driveGain = 1.0 + driveAmount * 3.0;
        double input = std::tanh((double)x * driveGain);

        // --- Drive-dependent resonance damping (Q decreases on large signals) ---
        double inputMag = std::abs(input);
        double resDamping = 1.0 / (1.0 + inputMag * kResonanceDampingCoeff);
        double effectiveReso = resonanceScaled * resDamping;

        // feedback (with unit delay)
        input -= effectiveReso * st.delay;

        // 4-stage cascaded 1-pole LPF — differential pair nonlinearity + mismatch via BJT_Primitive
        for (int i = 0; i < 4; ++i)
        {
            double s = st.s[i];
            // BJT primitive's integrate: gm mismatch + tanh saturation
            double y = stageBjt[i].integrate(input, s, g);
            st.s[i] = y;

            // inter-stage coupling capacitance (coupled via BJT primitive)
            if (i < 3)
            {
                y = stageBjt[i].interStageCoupling(y, st.interStageCap[i]);
                st.s[i] = y;
            }

            // Thermal noise injection (BJT material properties)
            double signalLevel = std::abs(y);
            if (signalLevel > 1e-10)
                y += normalDist(rng) * stageBjt[i].getSpec().thermalNoise * std::min(1.0, signalLevel);
            st.s[i] = FastMath::sanitize(y);

            input = y;
        }

        st.delay = st.s[3];
        return (float)st.s[3];
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
    static constexpr double kResonanceDampingCoeff  = 0.5;    // large-signal resonance damping strength
    static constexpr double kSupplyVoltageNominal   = 12.0;   // Nominal supply voltage (V)

    void updateCoefficients(float fc, float r, float temperature = 25.0f) noexcept
    {
        freq = std::clamp((double)fc, 20.0, sampleRate * 0.49);
        reso = std::clamp((double)r, 0.0, 1.0);
        lastTemp = temperature;

        // temperature-dependent cutoff: uses BJT_Primitive's temperature scale
        double tempScale = stageBjt[0].tempScale(temperature);
        double effectiveFreq = freq * tempScale;

        // Slight supply voltage fluctuation (±0.1% random)
        double supplyJitter = 1.0 + normalDist(rng) * 0.001;
        effectiveFreq *= supplyJitter;

        g = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * effectiveFreq / sampleRate);
        resonanceScaled = reso * 4.0;
    }

    struct ChannelState
    {
        double s[4] = {};
        double delay = 0.0;
        double interStageCap[3] = {}; // inter-stage coupling capacitance state
    };

    double sampleRate = PartsConstants::defaultSampleRate;
    double freq = 1000.0;
    double reso = 0.0;
    float lastTemp = 25.0f;
    double g = 0.0;
    double resonanceScaled = 0.0;
    std::vector<ChannelState> stage;
    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;
};
