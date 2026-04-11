#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"

// Classic spring reverb Type 4/8 model
// Transducer → multi-reflection delay network → pickup
// Drip effect (metallic trailing of impulse response)
// Diffusion via 3-spring configuration
class SpringReverbModel
{
public:
    struct Params
    {
        float decay = 0.5f;         // Decay 0.0–1.0
        float tone = 0.5f;          // Tone 0.0–1.0 (0=dark, 1=bright)
        float mix = 0.3f;           // Dry/Wet mix 0.0–1.0
        float tension = 0.5f;       // Spring tension 0.0–1.0 (affects reflection frequency)
        float dripAmount = 0.3f;    // Drip effect amount 0.0–1.0
        int numSprings = 3;         // Number of springs (1–3)
    };

    SpringReverbModel() noexcept = default;

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        numCh = std::max(1, numChannels);

        // Allocate delay buffers for 3 springs
        for (int s = 0; s < kMaxSprings; ++s)
        {
            // Delay per spring (30ms, 38ms, 47ms — distributed by golden ratio)
            const double baseMs = 30.0 + s * 8.5;
            springDelay[s].resize((size_t)(sr * baseMs * 0.001) + 2, 0.0f);
            springWritePos[s] = 0;

            // Allpass diffuser (2-stage)
            for (int d = 0; d < 2; ++d)
            {
                const double apMs = 5.0 + s * 2.5 + d * 3.7;
                apDelay[s][d].resize((size_t)(sr * apMs * 0.001) + 2, 0.0f);
                apWritePos[s][d] = 0;
                apState[s][d] = 0.0;
            }
        }

        // Cherry filter for drip (bandpass resonance)
        dripState.assign((size_t)numCh, {0.0, 0.0});
        lpfState.assign((size_t)numCh, 0.0);
        dcBlockX.assign((size_t)numCh, 0.0);
        dcBlockY.assign((size_t)numCh, 0.0);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (int s = 0; s < kMaxSprings; ++s)
        {
            std::fill(springDelay[s].begin(), springDelay[s].end(), 0.0f);
            springWritePos[s] = 0;
            for (int d = 0; d < 2; ++d)
            {
                std::fill(apDelay[s][d].begin(), apDelay[s][d].end(), 0.0f);
                apWritePos[s][d] = 0;
                apState[s][d] = 0.0;
            }
        }
        for (auto& ds : dripState) { ds.s1 = 0.0; ds.s2 = 0.0; }
        std::fill(lpfState.begin(), lpfState.end(), 0.0);
        std::fill(dcBlockX.begin(), dcBlockX.end(), 0.0);
        std::fill(dcBlockY.begin(), dcBlockY.end(), 0.0);
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, numCh - 1);
        const int springs = std::clamp(params.numSprings, 1, kMaxSprings);
        const double decay = 0.6 + std::clamp((double)params.decay, 0.0, 1.0) * 0.38; // 0.6–0.98

        double wet = 0.0;

        for (int s = 0; s < springs; ++s)
        {
            auto& buf = springDelay[s];
            if (buf.empty()) continue;
            const int bufSize = (int)buf.size();

            // Delay modulation by spring tension
            const double tensionScale = 0.8 + std::clamp((double)params.tension, 0.0, 1.0) * 0.4;
            const int readOffset = (int)((double)bufSize * tensionScale);
            int readPos = springWritePos[s] - std::clamp(readOffset, 1, bufSize - 1);
            if (readPos < 0) readPos += bufSize;

            const double delayed = buf[(size_t)readPos];

            // 2-stage allpass diffuser
            double diffused = delayed;
            for (int d = 0; d < 2; ++d)
            {
                auto& apBuf = apDelay[s][d];
                if (apBuf.empty()) continue;
                const int apSize = (int)apBuf.size();
                int apReadPos = apWritePos[s][d] - apSize + 1;
                if (apReadPos < 0) apReadPos += apSize;

                const double apCoeff = 0.5;  // Fixed dispersion coefficient
                const double apDelayed = apBuf[(size_t)(apReadPos % apSize)];
                const double apIn = diffused + apCoeff * apState[s][d];
                const double apOut = -apCoeff * apIn + apState[s][d];
                apState[s][d] = apDelayed;

                apBuf[(size_t)(apWritePos[s][d] % apSize)] = (float)apIn;
                apWritePos[s][d] = (apWritePos[s][d] + 1) % apSize;

                diffused = apOut;
            }

            // Feedback (write back with decay)
            buf[(size_t)springWritePos[s]] = (float)((double)x + diffused * decay);
            springWritePos[s] = (springWritePos[s] + 1) % bufSize;

            wet += diffused;
        }

        wet /= springs; // Normalize by number of springs

        // Drip effect (metallic resonance ~2.5kHz bandpass)
        if (params.dripAmount > 0.01f)
        {
            auto& ds = dripState[ch];
            const double dripFreq = 2500.0;
            const double dripQ = 8.0; // High Q for metallic character
            const double dripAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * dripFreq / sampleRate);

            const double hp = wet - ds.s2 - ds.s1 / dripQ;
            ds.s1 += dripAlpha * hp;
            ds.s2 += dripAlpha * ds.s1;

            wet += ds.s1 * params.dripAmount * 0.5; // Sum BP output
        }

        // Tone filter (1st-order LPF)
        {
            const double toneFreq = 800.0 + std::clamp((double)params.tone, 0.0, 1.0) * 7200.0; // 800–8000Hz
            const double toneAlpha = 1.0 - std::exp(-2.0 * 3.14159265358979323846 * toneFreq / sampleRate);
            lpfState[ch] += toneAlpha * (wet - lpfState[ch]);
            wet = lpfState[ch];
        }

        // DC block
        {
            const double dcAlpha = 0.995;
            double dcOut = dcAlpha * (dcBlockY[ch] + wet - dcBlockX[ch]);
            dcBlockX[ch] = wet;
            dcBlockY[ch] = dcOut;
            wet = dcOut;
        }

        // Dry/Wet Mix
        const double mix = std::clamp((double)params.mix, 0.0, 1.0);
        return (float)((double)x * (1.0 - mix) + wet * mix);
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

private:
    static constexpr int kMaxSprings = 3;

    struct DripState { double s1 = 0.0; double s2 = 0.0; };

    double sampleRate = PartsConstants::defaultSampleRate;
    int numCh = 1;

    // Spring delay lines (3)
    std::vector<float> springDelay[kMaxSprings];
    int springWritePos[kMaxSprings] = {};

    // All-pass diffuser (per spring x 2 stages)
    std::vector<float> apDelay[kMaxSprings][2];
    int apWritePos[kMaxSprings][2] = {};
    double apState[kMaxSprings][2] = {};

    // Drip resonance state
    std::vector<DripState> dripState;
    // Tone LPF state
    std::vector<double> lpfState;
    // DC block
    std::vector<double> dcBlockX;
    std::vector<double> dcBlockY;
};
