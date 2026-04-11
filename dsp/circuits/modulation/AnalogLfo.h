#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"

// TL072-style LFO model: triangle wave generation via integrator + Schmitt trigger
// provides triangle wave output [-1,1] and soft sine-like output via 1-pole LPF corner rounding
// emulates op-amp bandwidth limiting to smooth triangle wave corners
class AnalogLfo {
public:
    AnalogLfo() : rng(std::random_device{}()), normalDist(0.0, 1.0) {}
    ~AnalogLfo() = default;

    void prepare(int numChannels, double sampleRate) {
        fs = (sampleRate > 1.0 ? sampleRate : 44100.0);
        if (numChannels < 1) numChannels = 1;
        state.resize((size_t)numChannels);
        for (size_t ch = 0; ch < state.size(); ++ch) {
            // channel phase offset for stereo spread (approximately 90° offset)
            double seed = (ch % 2 == 0 ? -0.75 : 0.0);
            state[ch].tri = seed;
            state[ch].dir = (ch % 2 == 0 ? +1.0 : -1.0);
            state[ch].sinLike = seed;
            state[ch].sinLike2 = seed;
            state[ch].slopeDrift = 0.0;
            state[ch].thresholdDrift = 0.0;
        }
        recomputeInternal();
    }

    void prepare(const patina::ProcessSpec& spec) { prepare(spec.numChannels, spec.sampleRate); }

    void setRateHz(double hz) {
        rateHz = std::clamp(hz, 0.05, 20.0); // safe range limit: 0.05–20Hz
        recomputeInternal();
    }

    void setSupplyVoltage(double v) {
        // for threshold and bandwidth approximation biasing
        supplyV = std::clamp(v, 5.0, 36.0); // 5–36V limit
        recomputeInternal();
    }

    void reset() {
        for (auto& s : state) { 
            s.tri = -0.5; 
            s.dir = +1.0; 
            s.sinLike = -0.5;
            s.sinLike2 = -0.5;
            s.slopeDrift = 0.0;
            s.thresholdDrift = 0.0;
        }
    }

    // advance all channels by 1 sample
    inline void stepAll() noexcept {
        for (auto& s : state) stepOne(s);
    }

    // Per-channel accessors
    inline float getTri(int channel) const noexcept {
        if (channel < 0 || channel >= (int)state.size()) return 0.0f;
        return (float) std::clamp(state[(size_t)channel].tri, -1.0, 1.0);
    }
    inline float getSinLike(int channel) const noexcept {
        if (channel < 0 || channel >= (int)state.size()) return 0.0f;
        // normalizes 2-stage cascade LPF output by Schmitt threshold, ensuring ±1.0 range
        double normalized = state[(size_t)channel].sinLike2 / std::max(0.1, th);
        return (float) std::clamp(normalized, -1.0, 1.0);
    }

    // ensure consistency with current processing channel count
    void ensureChannels(int numChannels) {
        if ((int)state.size() == numChannels) return;
        prepare(numChannels, fs);
    }

private:
    struct ChState { 
        double tri = 0.0; 
        double dir = +1.0; 
        double sinLike = 0.0;
        double sinLike2 = 0.0;        // 2nd stage LPF state (classic analog synth-style cascade LPF)
        // low-speed random walk state for analog-like fluctuation
        double slopeDrift = 0.0;      // period fluctuation (equivalent to Wow & Flutter)
        double thresholdDrift = 0.0;  // Schmitt threshold fluctuation
    };
    std::vector<ChState> state;
    double fs = 44100.0;
    double rateHz = 0.5;
    double supplyV = 18.0;
    // derived parameters
    double slope = 0.0;          // per-sample triangle wave slope
    double th = 0.85;            // Schmitt threshold (full-scale ratio)
    double sinLpA = 0.0;         // 1st stage LPF alpha coefficient
    double sinLpA2 = 0.0;        // 2nd stage LPF alpha coefficient (classic analog synth cascade)
    
    // analog-like fluctuation parameters
    double driftSlopeAlpha = 0.0;     // LPF coefficient for slope drift
    double driftThresholdAlpha = 0.0; // LPF coefficient for threshold drift
    std::mt19937 rng;                 // random number generator
    std::normal_distribution<double> normalDist;

    inline void recomputeInternal() noexcept {
        slope = (4.0 * rateHz) / std::max(1.0, fs); // triangle wave from -1 to +1 within one period
        // rail-approach threshold at high supply voltage (op-amp headroom) maintained at 0.80–0.95
        double headroom = std::clamp(supplyV / PartsConstants::V_supplyMax, 0.3, 2.5);
        th = std::clamp(0.80 + 0.05 * std::log(headroom + 1e-6), 0.80, 0.95);
        
        // classic analog synth-style 2-stage cascade LPF setting (reproducing analog BBD chorus smoothness)
        // cutoff set lower to approach sine wave more closely (5–8x the LFO rate)
        // Example: 0.5Hz LFO → 1st stage 3.5Hz, 2nd stage 2.5Hz
        double fc1 = std::clamp(rateHz * 7.0, 2.0, 25.0);
        double RC1 = 1.0 / (2.0 * M_PI * fc1);
        double dt = 1.0 / std::max(1.0, fs);
        sinLpA = dt / (RC1 + dt);
        
        // 2nd stage: set even lower than 1st stage (0.7x) for a purer sine wave
        double fc2 = fc1 * 0.7;
        double RC2 = 1.0 / (2.0 * M_PI * fc2);
        sinLpA2 = dt / (RC2 + dt);
        
        // low-speed LPF coefficient for analog-like fluctuation (cutoff equivalent to approximately 0.05–0.2Hz)
        // mimics low-frequency fluctuations such as temperature drift and power supply ripple
        double driftFc = 0.1; // 0.1Hz = 10-second period fluctuation
        double driftRC = 1.0 / (2.0 * M_PI * driftFc);
        driftSlopeAlpha = dt / (driftRC + dt);
        
        // threshold fluctuation is even slower (approximately 0.05Hz = 20-second period)
        double thresholdDriftFc = 0.05;
        double thresholdDriftRC = 1.0 / (2.0 * M_PI * thresholdDriftFc);
        driftThresholdAlpha = dt / (thresholdDriftRC + dt);
    }

    inline void stepOne(ChState& s) noexcept {
        // low-speed random walk update (mimicking analog component temperature drift and power supply ripple)
        // approximately ±0.2% slope fluctuation (equivalent to Wow & Flutter)
        double slopeNoise = normalDist(rng) * 0.002;
        s.slopeDrift += driftSlopeAlpha * (slopeNoise - s.slopeDrift);
        
        // approximately ±0.5% threshold fluctuation (Schmitt trigger instability)
        double thresholdNoise = normalDist(rng) * 0.005;
        s.thresholdDrift += driftThresholdAlpha * (thresholdNoise - s.thresholdDrift);
        
        // effective slope and threshold (with fluctuation added)
        double effectiveSlope = slope * (1.0 + s.slopeDrift);
        double effectiveThreshold = th * (1.0 + s.thresholdDrift);
        effectiveThreshold = std::clamp(effectiveThreshold, 0.75, 0.98);
        
        // integrator update
        s.tri += s.dir * effectiveSlope;
        // hysteresis switching
        if (s.tri >= effectiveThreshold) { s.tri = effectiveThreshold; s.dir = -1.0; }     // switches to descending at upper limit
        else if (s.tri <= -effectiveThreshold) { s.tri = -effectiveThreshold; s.dir = +1.0; } // switches to ascending at lower limit
        
        // 2-stage cascade LPF processing (classic analog synth BBD chorus LFO)
        // 1st stage: triangle wave → rounded sine wave
        s.sinLike += sinLpA * (s.tri - s.sinLike);
        // 2nd stage: even smoother (-40dB/decade rolloff characteristic)
        s.sinLike2 += sinLpA2 * (s.sinLike - s.sinLike2);
    }
};
