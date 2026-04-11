#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"

// Classic plate reverb model
// Diffuse reverb based on FDN (Feedback Delay Network)
// Frequency-dependent decay, predelay, damping control
class PlateReverb
{
public:
    struct Params
    {
        float decay = 0.5f;         // Decay 0.0–1.0
        float predelayMs = 10.0f;   // Predelay (ms) 0–100
        float damping = 0.5f;       // Damping 0.0–1.0 (HF attenuation amount)
        float mix = 0.3f;           // Dry/Wet mix 0.0–1.0
        float diffusion = 0.7f;     // Diffusion amount 0.0–1.0
        float modDepth = 0.0f;      // Internal modulation depth 0.0–1.0
    };

    PlateReverb() noexcept = default;

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);

        // Predelay buffer (max 100ms)
        predelayBuf.assign((size_t)(sr * 0.1) + 2, 0.0f);
        predelayWritePos = 0;

        // FDN: 4 delay lines (prime-number sample lengths minimize correlation)
        // Classic plate reverb characteristics: metal plate vibration modes, dense reflection pattern
        static constexpr double fdnDelayMs[kFdnSize] = { 29.7, 37.1, 41.1, 43.7 };
        for (int i = 0; i < kFdnSize; ++i)
        {
            const int len = std::max(2, (int)(sr * fdnDelayMs[i] * 0.001));
            fdnDelay[i].assign((size_t)len, 0.0f);
            fdnWritePos[i] = 0;
            fdnDampState[i] = 0.0;
        }

        // Input diffuser (4-stage allpass)
        static constexpr double diffDelayMs[kDiffStages] = { 4.77, 3.59, 12.73, 9.31 };
        for (int i = 0; i < kDiffStages; ++i)
        {
            const int len = std::max(2, (int)(sr * diffDelayMs[i] * 0.001));
            diffDelay[i].assign((size_t)len, 0.0f);
            diffWritePos[i] = 0;
        }

        dcBlockX = 0.0;
        dcBlockY = 0.0;
        modPhase = 0.0;
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(predelayBuf.begin(), predelayBuf.end(), 0.0f);
        predelayWritePos = 0;
        for (int i = 0; i < kFdnSize; ++i)
        {
            std::fill(fdnDelay[i].begin(), fdnDelay[i].end(), 0.0f);
            fdnWritePos[i] = 0;
            fdnDampState[i] = 0.0;
        }
        for (int i = 0; i < kDiffStages; ++i)
        {
            std::fill(diffDelay[i].begin(), diffDelay[i].end(), 0.0f);
            diffWritePos[i] = 0;
        }
        dcBlockX = 0.0;
        dcBlockY = 0.0;
        modPhase = 0.0;
    }

    // Mono processing (in stereo mode, FDN taps are distributed across ch0/ch1)
    inline float process(int channel, float x, const Params& params) noexcept
    {
        // Predelay
        const int pdLen = (int)predelayBuf.size();
        predelayBuf[(size_t)predelayWritePos] = x;
        int pdReadOffset = std::clamp((int)(params.predelayMs * 0.001 * sampleRate), 1, pdLen - 1);
        int pdReadPos = predelayWritePos - pdReadOffset;
        if (pdReadPos < 0) pdReadPos += pdLen;
        const float predelayed = predelayBuf[(size_t)pdReadPos];
        if (channel == 0) // Write advances on ch0 only
            predelayWritePos = (predelayWritePos + 1) % pdLen;

        // Input diffuser (Schroeder allpass chain)
        const double diffCoeff = std::clamp((double)params.diffusion, 0.0, 0.95) * 0.75;
        double diffused = (double)predelayed;
        for (int i = 0; i < kDiffStages; ++i)
        {
            auto& buf = diffDelay[i];
            if (buf.empty()) continue;
            const int len = (int)buf.size();
            int rp = diffWritePos[i] - len + 1;
            if (rp < 0) rp += len;

            const double delayed = buf[(size_t)(rp % len)];
            const double apOut = -diffCoeff * diffused + delayed;
            buf[(size_t)(diffWritePos[i] % len)] = (float)(diffused + diffCoeff * apOut);
            if (channel == 0)
                diffWritePos[i] = (diffWritePos[i] + 1) % len;
            diffused = apOut;
        }

        // FDN: Hadamard-like mixing matrix
        const double decayGain = 0.3 + std::clamp((double)params.decay, 0.0, 1.0) * 0.68; // 0.3–0.98
        const double dampAlpha = std::clamp((double)params.damping, 0.0, 1.0) * 0.7; // Damping

        // Internal modulation (chorus-like fluctuation — simulating plate imperfections)
        double modOffset = 0.0;
        if (params.modDepth > 0.001f)
        {
            modOffset = std::sin(modPhase) * params.modDepth * 2.0;
            if (channel == 0)
                modPhase += 2.0 * 3.14159265358979323846 * 0.7 / sampleRate; // 0.7Hz
        }

        // FDN Readout
        double fdnOut[kFdnSize];
        for (int i = 0; i < kFdnSize; ++i)
        {
            auto& buf = fdnDelay[i];
            const int len = (int)buf.size();
            int rp = fdnWritePos[i] - len + 1 + (int)modOffset;
            while (rp < 0) rp += len;
            rp = rp % len;
            fdnOut[i] = buf[(size_t)rp];

            // Damping (1st-order LPF)
            fdnDampState[i] += (1.0 - dampAlpha) * (fdnOut[i] - fdnDampState[i]);
            fdnOut[i] = fdnDampState[i];
        }

        // Hadamard-like 4x4 mixing (unitary matrix approximation)
        const double s = 0.5; // Scaling (energy conservation)
        double mixed[kFdnSize];
        mixed[0] = s * ( fdnOut[0] + fdnOut[1] + fdnOut[2] + fdnOut[3]);
        mixed[1] = s * ( fdnOut[0] - fdnOut[1] + fdnOut[2] - fdnOut[3]);
        mixed[2] = s * ( fdnOut[0] + fdnOut[1] - fdnOut[2] - fdnOut[3]);
        mixed[3] = s * (-fdnOut[0] + fdnOut[1] + fdnOut[2] - fdnOut[3]);

        // FDN write (input + feedback)
        for (int i = 0; i < kFdnSize; ++i)
        {
            auto& buf = fdnDelay[i];
            const int len = (int)buf.size();
            buf[(size_t)(fdnWritePos[i] % len)] = (float)(diffused * 0.5 + mixed[i] * decayGain);
            if (channel == 0)
                fdnWritePos[i] = (fdnWritePos[i] + 1) % len;
        }

        // Output taps (stereo distribution: even channels use taps 0+2, odd use taps 1+3)
        double wet;
        if (channel & 1)
            wet = (fdnOut[1] + fdnOut[3]) * 0.5;
        else
            wet = (fdnOut[0] + fdnOut[2]) * 0.5;

        // DC block
        const double dcAlpha = 0.995;
        double dcOut = dcAlpha * (dcBlockY + wet - dcBlockX);
        dcBlockX = wet;
        dcBlockY = dcOut;
        wet = dcOut;

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
    static constexpr int kFdnSize = 4;
    static constexpr int kDiffStages = 4;

    double sampleRate = PartsConstants::defaultSampleRate;

    // Predelay
    std::vector<float> predelayBuf;
    int predelayWritePos = 0;

    // FDN delay lines
    std::vector<float> fdnDelay[kFdnSize];
    int fdnWritePos[kFdnSize] = {};
    double fdnDampState[kFdnSize] = {};

    // Diffuser
    std::vector<float> diffDelay[kDiffStages];
    int diffWritePos[kDiffStages] = {};

    // DC block
    double dcBlockX = 0.0;
    double dcBlockY = 0.0;

    // Internal modulation
    double modPhase = 0.0;
};
