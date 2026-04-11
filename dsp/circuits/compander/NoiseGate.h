#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"

// Classic dynamics analyzer / ISP Decimator-style analog noise gate
// Hysteresis-based open/close, sidechain filter, soft knee
// Follows CompanderModule structure
class NoiseGate
{
public:
    struct Params
    {
        float thresholdDb = -40.0f;     // Open/close threshold (dBFS)
        float hysteresisDb = 6.0f;      // Hysteresis width (dB)
        float attackMs = 0.5f;          // Attack time (ms)
        float holdMs = 50.0f;           // Hold time (ms)
        float releaseMs = 100.0f;       // Release time (ms)
        float range = 1.0f;             // Gate range 0.0–1.0（1.0=full mute, 0.5=-6dB）
        float sidechainHpHz = 100.0f;   // Sidechain HP cutoff (Hz)
    };

    NoiseGate() noexcept = default;

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        gateState.resize(nCh);
        for (auto& gs : gateState) gs = GateChannelState{};
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& gs : gateState) gs = GateChannelState{};
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)gateState.size() - 1);
        auto& gs = gateState[ch];

        // Sidechain HPF (low frequencies not used for gate triggering)
        const double hpFc = std::clamp((double)params.sidechainHpHz, 20.0, 5000.0);
        const double hpRc = 1.0 / (2.0 * 3.14159265358979323846 * hpFc);
        const double dt = 1.0 / sampleRate;
        const double hpAlpha = hpRc / (hpRc + dt);
        const double scInput = hpAlpha * (gs.scHpPrevY + (double)x - gs.scHpPrevX);
        gs.scHpPrevX = (double)x;
        gs.scHpPrevY = scInput;

        // Envelope detection (peak)
        const double absInput = std::abs(scInput);
        const double attAlpha = 1.0 - std::exp(-1.0 / (sampleRate * std::max(0.01, (double)params.attackMs * 0.001)));
        const double relAlpha = 1.0 - std::exp(-1.0 / (sampleRate * std::max(0.01, (double)params.releaseMs * 0.001)));
        if (absInput > gs.envelope)
            gs.envelope += attAlpha * (absInput - gs.envelope);
        else
            gs.envelope += relAlpha * (absInput - gs.envelope);

        // dB Conversion
        const double envDb = (gs.envelope > 1e-10)
            ? 20.0 * std::log10(gs.envelope)
            : -200.0;

        // Gate judgment with hysteresis
        const double openThresh = params.thresholdDb;
        const double closeThresh = params.thresholdDb - std::abs(params.hysteresisDb);

        if (!gs.isOpen && envDb > openThresh)
        {
            gs.isOpen = true;
            gs.holdCounter = (int)(sampleRate * std::max(0.0f, params.holdMs) * 0.001);
        }

        if (gs.isOpen && envDb < closeThresh)
        {
            if (gs.holdCounter > 0)
                --gs.holdCounter;
            else
                gs.isOpen = false;
        }
        else if (gs.isOpen)
        {
            gs.holdCounter = (int)(sampleRate * std::max(0.0f, params.holdMs) * 0.001);
        }

        // Gain smoothing
        const double targetGain = gs.isOpen ? 1.0 : (1.0 - std::clamp((double)params.range, 0.0, 1.0));
        const double gainAlpha = gs.isOpen ? attAlpha : relAlpha;
        gs.currentGain += gainAlpha * (targetGain - gs.currentGain);

        return (float)((double)x * gs.currentGain);
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

    bool isGateOpen(int channel) const noexcept
    {
        if (channel < 0 || (size_t)channel >= gateState.size()) return false;
        return gateState[(size_t)channel].isOpen;
    }

private:
    struct GateChannelState
    {
        double envelope = 0.0;
        double currentGain = 0.0;
        bool isOpen = false;
        int holdCounter = 0;
        // Sidechain HPF state
        double scHpPrevX = 0.0;
        double scHpPrevY = 0.0;
    };

    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<GateChannelState> gateState;
};
