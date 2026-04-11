#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/JFET_Primitive.h"

// Classic analog phaser-style JFET all-pass filter phaser (extended with BBD-level analog behavior)
// - 2–12 stage all-pass cascade
// - JFET Vp temperature coefficient (-2mV/°C → modulation characteristics change with temperature)
// - Per-stage gm variation (manufacturing tolerance — slightly different notch depth per stage)
// - JFET drain current saturation (soft clipping on large signals)
// - Filter delay in feedback path (finite bandwidth buffer)
// - Additional high-frequency phase rotation due to parasitic capacitance
//
// 4-layer architecture:
//   Parts: JFET_Primitive (2N5457) — gm/noise/temperature characteristics/saturation
//   → Circuit: PhaserStage (all-pass cascade + FB)
class PhaserStage
{
public:
    struct Params
    {
        float lfoValue = 0.0f;
        float depth = 0.5f;
        float feedback = 0.0f;
        float centerFreqHz = 1000.0f;
        float freqSpreadHz = 800.0f;
        int numStages = 4;
        float temperature = 25.0f;  // operating temperature (°C)
    };

    PhaserStage() noexcept : rng(67), normalDist(0.0, 1.0)
    {
        // each stage's JFET (2N5457) — individual variation assigned by different seeds
        for (int i = 0; i < kMaxStages; ++i)
            stageJfet[i] = JFET_Primitive(JFET_Primitive::N2N5457(), 211 + i * 31);
    }

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        states.resize(nCh);
        for (auto& ch : states)
        {
            std::fill(std::begin(ch.ap), std::end(ch.ap), 0.0);
            ch.fbSample = 0.0;
            ch.fbLpfState = 0.0;
            std::fill(std::begin(ch.parasiticCap), std::end(ch.parasiticCap), 0.0);
        }

        // bandwidth-limiting LPF on feedback path (~15kHz — buffer amplifier bandwidth)
        fbLpfAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * kFeedbackBandwidth / sampleRate);
        fbLpfAlpha = std::clamp(fbLpfAlpha, 0.1, 1.0);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& ch : states)
        {
            std::fill(std::begin(ch.ap), std::end(ch.ap), 0.0);
            ch.fbSample = 0.0;
            ch.fbLpfState = 0.0;
            std::fill(std::begin(ch.parasiticCap), std::end(ch.parasiticCap), 0.0);
        }
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)states.size() - 1);
        auto& st = states[ch];

        const int stages = std::clamp(params.numStages & ~1, 2, kMaxStages);
        const double fb = std::clamp((double)params.feedback, 0.0, 0.95);
        const double depth = std::clamp((double)params.depth, 0.0, 1.0);

        // --- JFET Vp temperature coefficient → modulation frequency shift (delegated to JFET primitive) ---
        double tempFreqScale = stageJfet[0].tempFreqScale(params.temperature);

        const double center = std::clamp((double)params.centerFreqHz, 100.0, sampleRate * 0.45);
        const double spread = std::clamp((double)params.freqSpreadHz, 0.0, center - 20.0);
        const double modFreq = (center + spread * depth * params.lfoValue) * tempFreqScale;
        const double clampedFreq = std::clamp(modFreq, 20.0, sampleRate * 0.49);

        const double t = std::tan(3.14159265358979323846 * clampedFreq / sampleRate);

        // feedback injection (bandwidth-limited)
        double fbSignal = st.fbSample;
        st.fbLpfState += fbLpfAlpha * (fbSignal - st.fbLpfState);
        double in = (double)x + st.fbLpfState * fb;

        // --- JFET drain current saturation (JFET primitive's soft clip) ---
        in = stageJfet[0].softClip(in);

        // all-pass cascade (with gm variation + parasitic capacitance per stage)
        for (int i = 0; i < stages; ++i)
        {
            // each stage's unique gm scale → all-pass coefficient differs slightly
            double gm = stageJfet[i].getGmScale();
            double ai = (t * gm - 1.0) / (t * gm + 1.0);

            double y = ai * in + st.ap[i];
            st.ap[i] = in - ai * y;

            // minor high-frequency phase rotation from parasitic capacitance (~100pF × Rd ≈ 1kΩ → fc ≈ 1.6MHz)
            // subtle analog characteristic effective during oversampling
            double& pc = st.parasiticCap[i];
            pc += kParasiticCapAlpha * (y - pc);
            y = y * 0.999 + pc * 0.001;

            in = y;
        }

        // JFET noise injection (1/f noise dominant — JFET primitive's physical property values)
        in += normalDist(rng) * stageJfet[0].getSpec().noiseLevel;

        st.fbSample = in;

        return (float)(x + in) * 0.5f;
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

private:
    static constexpr int kMaxStages = 12;

    // === circuit constants ===
    static constexpr double kParasiticCapAlpha = 0.3;     // parasitic capacitance LPF coefficient
    static constexpr double kFeedbackBandwidth = 15000.0; // FB path bandwidth (Hz)

    // === Component layer (Parts) ===
    JFET_Primitive stageJfet[12];  // each stage's JFET (2N5457, individual seed)

    struct ChannelState
    {
        double ap[12] = {};
        double fbSample = 0.0;
        double fbLpfState = 0.0;         // FB path bandwidth-limiting state
        double parasiticCap[12] = {};    // each stage's parasitic capacitance state
    };

    double sampleRate = PartsConstants::defaultSampleRate;
    double fbLpfAlpha = 1.0;
    std::vector<ChannelState> states;
    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;
};
