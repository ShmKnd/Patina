#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/TapePrimitive.h"

// Studio tape machine-style tape saturation emulation (extended with BBD-level analog behavior)
// - Magnetic hysteresis (simplified Jiles-Atherton）
// - Head bump (LF resonance — tape speed dependent)
// - HF Self-erase (high-frequency rolloff)
// - Wow & flutter (random + periodic components)
// - Gap loss (HF attenuation due to playback head gap length)
// - Demagnetization degradation (bias noise increase from head magnetization)
// - Tape hiss noise (1/f + white noise blend)
// - Azimuth error (stereo phase misalignment)
//
// 4-layer architecture:
//   Parts: TapePrimitive (HighSpeedDeck) — Hysteresis / gap loss / noise
//   → Circuit: TapeSaturation (W&F + head bump + HF rolloff)
class TapeSaturation
{
public:
    struct Params
    {
        float inputGain = 0.0f;
        float saturation = 0.5f;
        float biasAmount = 0.5f;
        float tapeSpeed = 1.0f;      // 0.5=7.5ips, 1.0=15ips, 2.0=30ips
        float wowFlutter = 0.0f;
        bool enableHeadBump = true;
        bool enableHfRolloff = true;
        float headWear = 0.0f;       // Head wear 0.0–1.0（Increased gap loss）
        float tapeAge = 0.0f;        // Tape degradation 0.0–1.0（Hiss noise + HF loss）
    };

    TapeSaturation() noexcept
        : rng(97), normalDist(0.0, 1.0), pinkState{0.0, 0.0, 0.0},
          tape(TapePrimitive::HighSpeedDeck())
    {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        tapeState.assign(nCh, 0.0);
        headBumpState.assign(nCh, {0.0, 0.0});
        hfState.assign(nCh, 0.0);
        gapLossState.assign(nCh, 0.0);
        prevSample.assign(nCh, 0.0);
        wfPhase = 0.0;
        // State for wow & flutter random variation
        wfRandomState = 0.0;
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(tapeState.begin(), tapeState.end(), 0.0);
        for (auto& s : headBumpState) { s.s1 = 0.0; s.s2 = 0.0; }
        std::fill(hfState.begin(), hfState.end(), 0.0);
        std::fill(gapLossState.begin(), gapLossState.end(), 0.0);
        std::fill(prevSample.begin(), prevSample.end(), 0.0);
        wfPhase = 0.0;
        wfRandomState = 0.0;
        pinkState[0] = pinkState[1] = pinkState[2] = 0.0;
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)tapeState.size() - 1);

        const double inGain = std::pow(10.0, std::clamp((double)params.inputGain, -12.0, 12.0) / 20.0);
        double v = (double)x * inGain;

        // --- Wow & flutter (periodic + random components)---
        if (params.wowFlutter > 0.001f)
        {
            const double wfAmount = params.wowFlutter * 0.003;
            const double wowRate = 0.5;
            const double flutterRate = 6.0;
            const double wow = std::sin(2.0 * 3.14159265358979323846 * wowRate * wfPhase / sampleRate) * wfAmount * 0.6;
            const double flutter = std::sin(2.0 * 3.14159265358979323846 * flutterRate * wfPhase / sampleRate) * wfAmount * 0.25;
            // Random component (irregular variation in tape transport mechanism — scrape flutter)
            double scrape = 0.0;
            if (channel == 0)
            {
                wfRandomState = wfRandomState * 0.999 + normalDist(rng) * wfAmount * 0.15;
                wfPhase += 1.0;
            }
            scrape = wfRandomState;
            v *= (1.0 + wow + flutter + scrape);
        }

        // --- Tape hiss noise (delegated to TapePrimitive)---
        {
            double white = normalDist(rng);
            double hiss = tape.generateHiss(white, (double)params.tapeAge);
            v += hiss;
        }

        // --- Tape magnetic hysteresis (delegated to TapePrimitive)---
        {
            double coercivity = 1.0 - 0.3 * (double)params.tapeAge;
            v = tape.processHysteresis(v, (double)params.saturation, coercivity);

            if (params.biasAmount < 0.9f)
            {
                double input = v;
                v += (input - v) * (1.0 - params.biasAmount) * 0.1;
            }
        }

        // --- Head bump ---
        if (params.enableHeadBump)
        {
            const double bumpFreq = 60.0 * std::clamp((double)params.tapeSpeed, 0.5, 2.0);
            const double bumpAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * bumpFreq / sampleRate);
            const double bumpQ = 2.0;

            auto& hb = headBumpState[ch];
            const double hp = v - hb.s2 - hb.s1 / bumpQ;
            hb.s1 += bumpAlpha * hp;
            hb.s2 += bumpAlpha * hb.s1;
            v += hb.s2 * 0.26;
        }

        // --- Gap loss (delegated to TapePrimitive)---
        {
            double gapLossFreq = tape.gapLossFc((double)params.tapeSpeed, (double)params.headWear);
            gapLossFreq = std::clamp(gapLossFreq, 5000.0, sampleRate * 0.49);

            double glAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * gapLossFreq / sampleRate);
            double& gl = gapLossState[ch];
            gl += glAlpha * (v - gl);
            v = gl;
        }

        // --- HF Self-erase ---
        if (params.enableHfRolloff)
        {
            // Tape deterioration: HF cutoff drops further
            double agingFactor = 1.0 - 0.3 * (double)params.tapeAge;
            const double hfCutoff = 10000.0 * std::clamp((double)params.tapeSpeed, 0.5, 2.0) * agingFactor;
            const double hfA = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * hfCutoff / sampleRate);
            double& hfs = hfState[ch];
            hfs += hfA * (v - hfs);
            v = hfs;
        }

        // --- Demagnetization degradation (TapePrimitive physical properties)---
        if (params.headWear > 0.1f)
        {
            double dcBias = (double)params.headWear * tape.getSpec().demagDcBias;
            double demagNoise = normalDist(rng) * (double)params.headWear * tape.getSpec().demagNoise;
            v += dcBias + demagNoise;
        }

        prevSample[ch] = v;
        v /= inGain;

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
    TapePrimitive tape;  // High-speed studio deck

    struct HeadBumpState { double s1 = 0.0; double s2 = 0.0; };

    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<double> tapeState;
    std::vector<HeadBumpState> headBumpState;
    std::vector<double> hfState;
    std::vector<double> gapLossState;       // Gap loss LPF state
    std::vector<double> prevSample;         // Previous sample
    double wfPhase = 0.0;
    double wfRandomState = 0.0;             // W&F Random state
    double pinkState[3];                    // Pink noise state

    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;
};
