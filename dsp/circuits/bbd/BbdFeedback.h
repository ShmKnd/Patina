#pragma once
#include <algorithm>
#include <vector>
#include <random>
#include <cmath>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"
#include "../../parts/OpAmpPrimitive.h"
// #include "../utils/FastMath.h"  // benchmark result: reverted to std::tanh since overall plugin difference is ~0.4%

// local PI constant — cross-platform support by avoiding M_PI macro dependency
constexpr double kPi = 3.141592653589793238462643383279502884;
// M_PI definition — for compatibility with other headers and translation units
#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884
#endif

// feedback module — feedback path model of a real analog delay
// DC removal, all-pass filter (phase rotation), parasitic capacitance, op-amp nonlinearity, noise injection
class BbdFeedback
{
public:
    BbdFeedback() noexcept : sampleRate(PartsConstants::defaultSampleRate) {}
    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = sr;
        fbHpXPrev.assign((size_t)numChannels, 0.0);      // DC removal HP previous input
        fbHpYPrev.assign((size_t)numChannels, 0.0);      // DC removal HP previous output
        fbAllpassXPrev.assign((size_t)numChannels, 0.0); // all-pass filter previous input
        fbAllpassYPrev.assign((size_t)numChannels, 0.0); // all-pass filter previous output
        fbParasiticPrev.assign((size_t)numChannels, 0.0); // parasitic capacitance LPF previous output
        noiseHpXPrev.assign((size_t)numChannels, 0.0);   // noise HP previous input
        noiseHpYPrev.assign((size_t)numChannels, 0.0);   // noise HP previous output

        // prepare OpAmpPrimitive instance for each channel
        opAmps_.resize((size_t)numChannels);
        for (auto& amp : opAmps_)
        {
            amp = OpAmpPrimitive(opAmpSpec_);
            amp.prepare(sr);
        }
    }
    void prepare(const patina::ProcessSpec& spec) noexcept { prepare(spec.numChannels, spec.sampleRate); }
    void reset() noexcept
    {
        std::fill(fbHpXPrev.begin(), fbHpXPrev.end(), 0.0);
        std::fill(fbHpYPrev.begin(), fbHpYPrev.end(), 0.0);
        std::fill(fbAllpassXPrev.begin(), fbAllpassXPrev.end(), 0.0);
        std::fill(fbAllpassYPrev.begin(), fbAllpassYPrev.end(), 0.0);
        std::fill(fbParasiticPrev.begin(), fbParasiticPrev.end(), 0.0);
        std::fill(noiseHpXPrev.begin(), noiseHpXPrev.end(), 0.0);
        std::fill(noiseHpYPrev.begin(), noiseHpYPrev.end(), 0.0);
        for (auto& amp : opAmps_) amp.reset();
    }

    // feedback integrated configuration
    struct Config
    {
        bool emulateOpAmpSaturation = true;
        double opAmpNoiseGain = 1.0;
        double bbdNoiseLevel = PartsConstants::bbdBaseNoise;
        bool highVoltageMode = false;
        double capacitanceScale = 1.0;
    };

    inline float process(int channel, float rawFb, bool emulateOpAmpSaturation,
                         std::minstd_rand& rng, std::normal_distribution<double>& normalDist,
                         double opAmpNoiseGain, double bbdNoiseLevel, double currentSampleRate,
                         bool highVoltageMode = false, double capacitanceScale = 1.0) noexcept
    {
        if (channel < 0 || (size_t)channel >= fbHpXPrev.size()) return rawFb;

        // === Operating point change from supply voltage ===
        // 9V: vintage mode (strong compression, warmth)
        // 18V: high headroom mode (clean, wide dynamics)
        double noiseFloorScale;
        if (highVoltageMode)
            noiseFloorScale = PartsConstants::opAmp_noiseScale18V;
        else
            noiseFloorScale = PartsConstants::opAmp_noiseScale9V;

        const double dt = 1.0 / currentSampleRate;

        // step 1: DC removal 1st order RC high-pass filter
        // using existing parts: C_inputCoupling (4.7µF) + R_input (1MΩ) → fc ≈ 33.9Hz
        // aging effect: as capacitance decreases, response shifts toward highs, DC removal frequency rises
        const double RC_dcBlock = PartsConstants::R_input * (PartsConstants::C_inputCoupling * capacitanceScale);
        double hpAlpha = RC_dcBlock / (RC_dcBlock + dt);
        double prevX_hp = fbHpXPrev[(size_t)channel];
        double prevY_hp = fbHpYPrev[(size_t)channel];
        double hpOut = hpAlpha * (prevY_hp + static_cast<double>(rawFb) - prevX_hp);
        fbHpXPrev[(size_t)channel] = static_cast<double>(rawFb);
        fbHpYPrev[(size_t)channel] = hpOut;

        // step 2: all-pass filter (phase rotation only, amplitude passes through)
        // using existing parts: C_tone (22nF) + R_driveHp (12kΩ) → fc ≈ 602Hz
        // real hardware: phase delays in inverting amplifier/buffer stages cumulatively produce tonal changes
        // aging effect: frequency characteristics of phase rotation change with capacitance decrease
        const double RC_allpass = PartsConstants::R_driveHp * (PartsConstants::C_tone * capacitanceScale);
        const double wc_allpass = 1.0 / RC_allpass;
        double apAlpha = (1.0 - wc_allpass * dt) / (1.0 + wc_allpass * dt);
        double prevX_ap = fbAllpassXPrev[(size_t)channel];
        double prevY_ap = fbAllpassYPrev[(size_t)channel];
        double apOut = apAlpha * (hpOut - prevY_ap) + prevX_ap;
        fbAllpassXPrev[(size_t)channel] = hpOut;
        fbAllpassYPrev[(size_t)channel] = apOut;

        // step 3: ultra-high frequency rolloff due to parasitic capacitance
        // using existing parts: C_clock (100pF) + R_tonePot (100kΩ) → fc ≈ 15.9kHz
        // unintended LPF from wiring, PCB, and op-amp input capacitance
        // aging effect: parasitic capacitance has little age-related change but is slightly affected
        const double RC_parasitic = PartsConstants::R_tonePot * (PartsConstants::C_clock * capacitanceScale);
        const double fc_parasitic = 1.0 / (2.0 * kPi * RC_parasitic);
        double parasiticAlpha = 1.0 - std::exp(-2.0 * kPi * fc_parasitic * dt);
        parasiticAlpha = std::clamp(parasiticAlpha, 1e-12, 0.999999999);
        double prevParasitic = fbParasiticPrev[(size_t)channel];
        double parasiticOut = parasiticAlpha * apOut + (1.0 - parasiticAlpha) * prevParasitic;
        fbParasiticPrev[(size_t)channel] = parasiticOut;

        float fbProcessed = static_cast<float>(parasiticOut);

        // step 4: op-amp nonlinearity (slew rate limiting + input offset + saturation)
        if (emulateOpAmpSaturation)
        {
            double normalized = static_cast<double>(fbProcessed);
            
            // 4a. slew rate limiting — delegated to OpAmpPrimitive
            normalized = opAmps_[(size_t)channel].applySlewLimit(normalized);
            
            // 4b. input offset voltage (random walk dependent on temperature and voltage)
            // scaled up from thermalNoise (1nV) baseline (real hardware individual variation and temperature drift)
            double offsetDrift = normalDist(rng) * PartsConstants::thermalNoise * PartsConstants::opAmp_offsetDriftScale * opAmpSpec_.noiseScale;
            normalized += offsetDrift * noiseFloorScale;
            
            // 4c. Soft saturation — delegated to OpAmpPrimitive (supply voltage dependent + positive/negative asymmetry)
            normalized = opAmps_[(size_t)channel].saturate(normalized, highVoltageMode);
            
            fbProcessed = static_cast<float>(normalized);
        }

        // step 5: injection of BBD-specific noise (naturally accumulates with each repeat)
        if (bbdNoiseLevel > 0.0)
        {
            double rawN = normalDist(rng) * bbdNoiseLevel * noiseFloorScale;
            // high-frequency noise shaping (bbdNoiseHpFc: emphasizing above 6kHz)
            const double noiseHpFc = PartsConstants::bbdNoiseHpFc;
            double RC = 1.0 / (2.0 * kPi * noiseHpFc);
            double hpAlpha_n = RC / (RC + dt);
            double prevX = noiseHpXPrev[(size_t)channel];
            double prevY = noiseHpYPrev[(size_t)channel];
            double hp = hpAlpha_n * (prevY + rawN - prevX);
            noiseHpXPrev[(size_t)channel] = rawN;
            noiseHpYPrev[(size_t)channel] = hp;
            const double noiseInjectScale = PartsConstants::bbdNoiseInjectionGain;
            fbProcessed += static_cast<float>(hp * noiseInjectScale);
        }

        // step 6: op-amp thermal noise (noticeable on small signals)
        if (opAmpNoiseGain > 1.0)
        {
            double thermalN = normalDist(rng) * PartsConstants::thermalNoise * (opAmpNoiseGain - 1.0) * noiseFloorScale;
            fbProcessed += static_cast<float>(thermalN * 1e3);
        }

        // Fixed gain of op-amp buffer stage (calculated from PartsConstants resistance values)
        // non-inverting amplifier configuration: G = 1 + R_fbBufferRf / R_fbBufferRg
        // Small gain to compensate BBD insertion loss + parasitic capacitance filter loss
        constexpr float kOpAmpBufferGain = static_cast<float>(PartsConstants::fbBufferGain);
        fbProcessed *= kOpAmpBufferGain;

        return fbProcessed;
    }

    // block processing API
    void processBlock(float* const* io, int numChannels, int numSamples,
                      const Config& config,
                      std::minstd_rand& rng, std::normal_distribution<double>& normalDist) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i],
                                    config.emulateOpAmpSaturation,
                                    rng, normalDist,
                                    config.opAmpNoiseGain,
                                    config.bbdNoiseLevel,
                                    sampleRate,
                                    config.highVoltageMode,
                                    config.capacitanceScale);
    }

private:
    double sampleRate;
    std::vector<double> fbHpXPrev, fbHpYPrev;           // DC removal HP filter history
    std::vector<double> fbAllpassXPrev, fbAllpassYPrev; // all-pass filter history
    std::vector<double> fbParasiticPrev;                // parasitic capacitance LPF history
    std::vector<double> noiseHpXPrev, noiseHpYPrev;     // noise HP filter history

public:
    // MOD override: unified to OpAmpPrimitive::Spec (replaces old OpAmpOverrides)
    using OpAmpOverrides = OpAmpPrimitive::Spec;  // backward compatibility alias
    void setOpAmpOverrides(const OpAmpPrimitive::Spec& s) noexcept
    {
        opAmpSpec_ = s;
        // hot-swap existing instances' Spec
        for (auto& amp : opAmps_)
            amp = OpAmpPrimitive(s);
        // slew rate state is reset; same as swapping ICs in real hardware
        for (auto& amp : opAmps_)
            amp.prepare(sampleRate);
    }

private:
    OpAmpPrimitive::Spec opAmpSpec_;
    std::vector<OpAmpPrimitive> opAmps_;              // per-channel op-amp instances
};
