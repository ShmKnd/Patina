/*
  ==============================================================================

    CompanderModule.h
    Created: 26 Sep 2025
    Author:  GitHub Copilot

    BBD-style analog compander simulation — full-featured bucket-brigade delay compression processing
    
  ==============================================================================
*/

#pragma once
#include <vector>
#include <random>
#include <limits>
#include <cmath>
#include <algorithm>
#include "../../constants/PartsConstants.h"
#include "../../core/AudioCompat.h"
#include "../../core/ProcessSpec.h"

// ============================================================================
// Compander parameter constants (derived from NE570/SA571 external component values)
// ============================================================================
namespace CompanderParams
{
    // === Compressor (pre-BBD stage)===
    constexpr float COMP_THRESHOLD_VRMS = static_cast<float>(PartsConstants::V_ref_NE570);  // NE570 internal reference 316mVrms
    constexpr float COMP_RATIO = 2.0f;               // Compression ratio (2:1 = NE570 standard)
    constexpr float COMP_KNEE_DB = 6.0f;             // Soft knee width (dB)
    constexpr double COMP_ATTACK_SEC = PartsConstants::ne570AttackSec;   // C_T×V_ref/(2×I_charge) ≈ 10.7ms
    constexpr double COMP_RELEASE_SEC = PartsConstants::ne570ReleaseSec; // R_T×C_T ≈ 81.6ms
    
    // === Expander (post-BBD stage) — NE570 operates symmetrically with same RC circuit ===
    constexpr float EXP_THRESHOLD_VRMS = static_cast<float>(PartsConstants::V_ref_NE570);
    constexpr float EXP_RATIO = 2.0f;                // NE570 symmetric (2:1 ↔ 1:2)
    constexpr float EXP_KNEE_DB = 6.0f;              // Soft knee width (dB)
    constexpr double EXP_ATTACK_SEC = PartsConstants::ne570AttackSec;    // Same RC → symmetric
    constexpr double EXP_RELEASE_SEC = PartsConstants::ne570ReleaseSec;  // Same RC → symmetric
    
    // === Common parameters ===
    constexpr double RMS_TIME_CONSTANT_SEC = PartsConstants::ne570RmsTimeSec; // R_det×C_det = 10ms
    constexpr float SIDECHAIN_REF_VRMS = static_cast<float>(PartsConstants::V_ref_NE570);
    
    // === Pre/de-emphasis filter (NE570 external RC) ===
    constexpr bool PRE_DE_EMPHASIS_ENABLED = true;
    constexpr double PRE_DE_EMPHASIS_FC_HZ = PartsConstants::ne570EmphasisFcHz; // R_pre×C_pre → ≈2487Hz
    constexpr double PRE_EMPHASIS_RC_SCALE = 1.0;    // NE570: 1.0 because same RC circuit
    constexpr double DE_EMPHASIS_RC_SCALE = 1.0;     // NE570: 1.0 because same RC circuit
    constexpr double PRE_EMPHASIS_MAKEUP_GAIN = PartsConstants::compPreEmphasisGain;  // HP insertion loss compensation
    constexpr double DE_EMPHASIS_MAKEUP_GAIN = PartsConstants::compDeEmphasisGain;    // LP insertion loss compensation
    
    // === VCA noise ===
    constexpr double VCA_NOISE_LEVEL = 1e-5;         // VCA noise floor
}

// Compander module — NE570-style BBD compander circuit simulation
class CompanderModule
{
public:
    struct TimingConfig
    {
        double compressorAttack = CompanderParams::COMP_ATTACK_SEC;
        double compressorRelease = CompanderParams::COMP_RELEASE_SEC;
        double expanderAttack = CompanderParams::EXP_ATTACK_SEC;
        double expanderRelease = CompanderParams::EXP_RELEASE_SEC;
        double rmsTimeConstant = CompanderParams::RMS_TIME_CONSTANT_SEC;
    };

    struct Config
    {
        float thresholdVrms = CompanderParams::COMP_THRESHOLD_VRMS;
        float sidechainRefVrms = CompanderParams::SIDECHAIN_REF_VRMS;
        float ratio = CompanderParams::COMP_RATIO;
        float kneeDb = CompanderParams::COMP_KNEE_DB;
        bool preDeEmphasis = CompanderParams::PRE_DE_EMPHASIS_ENABLED;
        double preDeEmphasisFcHz = CompanderParams::PRE_DE_EMPHASIS_FC_HZ;
        double preEmphasisRcScale = CompanderParams::PRE_EMPHASIS_RC_SCALE;
        double deEmphasisRcScale = CompanderParams::DE_EMPHASIS_RC_SCALE;
        double preEmphasisMakeupGain = CompanderParams::PRE_EMPHASIS_MAKEUP_GAIN;
        double deEmphasisMakeupGain = CompanderParams::DE_EMPHASIS_MAKEUP_GAIN;
        double vcaOutputRatio = PartsConstants::ne570VcaOutputRatio;
        TimingConfig timing;
    };

    CompanderModule() = default;
    ~CompanderModule() = default;

    inline void prepare(int numChannels, double sampleRate)
    {
        currentSampleRate = sampleRate;
        channels.clear();
        channels.resize(static_cast<size_t>(numChannels));
        recomputefilter();
        reset();
    }

    void prepare(const patina::ProcessSpec& spec) { prepare(spec.numChannels, spec.sampleRate); }

    inline void reset()
    {
        for (auto& ch : channels)
        {
            ch.env = 0.0;
            ch.envSq = 0.0;
            ch.vcaState = 1.0;
            ch.vcaNoise = 0.0;
            ch.hpXPrev = ch.hpYPrev = 0.0;
            ch.lpXPrev = ch.lpYPrev = 0.0;
        }
    }
    
    // For dBV optimization: threshold, reference (Vrms), ratio,Knee settings
    void setThresholdVrms(float vrms) noexcept { thresholdVrms = std::max(1.0e-9f, vrms); }
    void setSidechainRefVrms(float vrms) noexcept { sidechainRefVrms = std::max(1.0e-9f, vrms); }
    void setRatio(float r) noexcept { userRatio = std::clamp(r, 1.0f, 4.0f); } // NE570 specification: approx. 2:1
    void setKnee(float k) noexcept { userKneeDb = std::clamp(k, 0.0f, 12.0f); } // dB width limit
    // Pre/de-emphasis (simplified approximation of NE570 peripheral circuit)
    void setPreDeEmphasis(bool enable, double fcHz = 3000.0) noexcept { 
        preDeEnabled = enable; 
        fcPreDeHz = std::clamp(fcHz, 200.0, 12000.0); 
        recomputefilter(); 
    }
    
    // Advanced tuning: Set RC scale factors and makeup gains dynamically
    void setPreEmphasisRcScale(double scale) noexcept {
        preRcScale = std::clamp(scale, 0.1, 10.0);
        recomputefilter();
    }
    void setDeEmphasisRcScale(double scale) noexcept {
        deRcScale = std::clamp(scale, 0.1, 10.0);
        recomputefilter();
    }
    void setPreEmphasisMakeupGain(double gain) noexcept {
        preGain = std::clamp(gain, 0.1, 20.0);
    }
    void setDeEmphasisMakeupGain(double gain) noexcept {
        deGain = std::clamp(gain, 0.1, 20.0);
    }
    
    inline void setTimingConfig(const TimingConfig& config) noexcept
    {
        auto sanitise = [](double value, double fallback)
        {
            if (!std::isfinite(value)) return fallback;
            return std::clamp(value, 1.0e-6, 10.0);
        };

        timingConfig.compressorAttack = sanitise(config.compressorAttack, timingConfig.compressorAttack);
        timingConfig.compressorRelease = sanitise(config.compressorRelease, timingConfig.compressorRelease);
        timingConfig.expanderAttack = sanitise(config.expanderAttack, timingConfig.expanderAttack);
        timingConfig.expanderRelease = sanitise(config.expanderRelease, timingConfig.expanderRelease);
        timingConfig.rmsTimeConstant = sanitise(config.rmsTimeConstant, timingConfig.rmsTimeConstant);
    }
    TimingConfig getTimingConfig() const noexcept { return timingConfig; }
    
    // MOD: VCA output efficiency override (NE570=0.6, SA571=0.65, etc.)
    void setVcaOutputRatio(double ratio) noexcept { vcaOutputRatioOverride = std::clamp(ratio, 0.3, 1.0); }
    
    void setConfig(const Config& cfg) noexcept
    {
        setThresholdVrms(cfg.thresholdVrms);
        setSidechainRefVrms(cfg.sidechainRefVrms);
        setRatio(cfg.ratio);
        setKnee(cfg.kneeDb);
        setPreDeEmphasis(cfg.preDeEmphasis, cfg.preDeEmphasisFcHz);
        setPreEmphasisRcScale(cfg.preEmphasisRcScale);
        setDeEmphasisRcScale(cfg.deEmphasisRcScale);
        setPreEmphasisMakeupGain(cfg.preEmphasisMakeupGain);
        setDeEmphasisMakeupGain(cfg.deEmphasisMakeupGain);
        setVcaOutputRatio(cfg.vcaOutputRatio);
        setTimingConfig(cfg.timing);
    }

    Config getConfig() const noexcept
    {
        Config cfg;
        cfg.thresholdVrms = thresholdVrms;
        cfg.sidechainRefVrms = sidechainRefVrms;
        cfg.ratio = userRatio;
        cfg.kneeDb = userKneeDb;
        cfg.preDeEmphasis = preDeEnabled;
        cfg.preDeEmphasisFcHz = fcPreDeHz;
        cfg.preEmphasisRcScale = preRcScale;
        cfg.deEmphasisRcScale = deRcScale;
        cfg.preEmphasisMakeupGain = preGain;
        cfg.deEmphasisMakeupGain = deGain;
        cfg.vcaOutputRatio = vcaOutputRatioOverride;
        cfg.timing = timingConfig;
        return cfg;
    }

    // NE570-style compression processing (pre-BBD stage)
    inline float processCompress(int channel, float input, float compAmount, float* appliedGain = nullptr)
    {
        if (channel < 0 || channel >= static_cast<int>(channels.size()))
            return input;
        
        if (compAmount <= 0.0f)
            return input;

        auto& ch = channels[(size_t)channel];

        // Pre-emphasis (HP) to push HF above noise pre-BBD
        double x = static_cast<double>(input);
        if (preDeEnabled)
        {
            double y = hpA * (ch.hpYPrev + x - ch.hpXPrev);
            ch.hpXPrev = x; ch.hpYPrev = y;
            x = y * preGain;
        }

        // Sidechain RMS detector
        const double rmsAlpha = timeToAlpha(timingConfig.rmsTimeConstant);
        ch.envSq = (1.0 - rmsAlpha) * ch.envSq + rmsAlpha * (x * x);
        const double env = std::sqrt(ch.envSq + 1.0e-18);

        // Compute target gain in dB (compressor)
        const double levelDb = linearToDb(env / std::max(1.0e-12, (double)sidechainRefVrms));
        const double thresholdDb = linearToDb(std::max(1.0e-12, (double)thresholdVrms / (double)sidechainRefVrms));
        double gainDb = computeGainDbWithKnee(levelDb, thresholdDb, userRatio, userKneeDb, true);
        gainDb *= static_cast<double>(compAmount);
        double targetGain = dbToLinear(gainDb);

        // Attack/Release smoothing on VCA control
        const double aAtk = timeToAlpha(timingConfig.compressorAttack);
        const double aRel = timeToAlpha(timingConfig.compressorRelease);
        if (targetGain < ch.vcaState) ch.vcaState = aAtk * targetGain + (1.0 - aAtk) * ch.vcaState;
        else                          ch.vcaState = aRel * targetGain + (1.0 - aRel) * ch.vcaState;

        // Apply VCA nonlinearity and noise
        const float out = applyVCA(static_cast<float>(x), ch.vcaState, ch.vcaState, ch.vcaNoise, ch.noiseRng);
        if (appliedGain != nullptr)
            *appliedGain = (float) std::clamp(ch.vcaState, 1.0e-4, 1.0);
        
        return out;
    }
    
    // NE570-style expansion processing (post-BBD stage)
    inline float processExpand(int channel, float input, float compAmount)
    {
        return processExpand(channel, input, compAmount, input, std::numeric_limits<float>::quiet_NaN());
    }

    inline float processExpand(int channel, float input, float compAmount, float detectorSample, float linkedGainOverride = std::numeric_limits<float>::quiet_NaN())
    {
        if (channel < 0 || channel >= static_cast<int>(channels.size()))
            return input;
        
        if (compAmount <= 0.0f)
            return input;

        auto& ch = channels[(size_t)channel];

        double x = static_cast<double>(input);
        double detect = static_cast<double>(detectorSample);

        const double amount = std::clamp((double) compAmount, 0.0, 1.0);
        double targetGain = 1.0;

        if (std::isfinite(linkedGainOverride))
        {
            const double linked = std::clamp((double) linkedGainOverride, 1.0e-4, 1.0);
            const double desired = 1.0 / linked;
            targetGain = std::clamp(desired, 1.0, 256.0);
            const double rmsAlpha = timeToAlpha(timingConfig.rmsTimeConstant);
            ch.envSq = (1.0 - rmsAlpha) * ch.envSq + rmsAlpha * (detect * detect);
        }
        else
        {
            const double rmsAlpha = timeToAlpha(timingConfig.rmsTimeConstant);
            ch.envSq = (1.0 - rmsAlpha) * ch.envSq + rmsAlpha * (detect * detect);
            const double env = std::sqrt(ch.envSq + 1.0e-18);

            const double levelDb = linearToDb(env / std::max(1.0e-12, (double)sidechainRefVrms));
            const double thresholdDb = linearToDb(std::max(1.0e-12, (double)thresholdVrms / (double)sidechainRefVrms));

            double gainDb = 0.0;
            if (levelDb < thresholdDb)
            {
                const double halfKnee = (double) userKneeDb * 0.5;
                const double under = thresholdDb - levelDb;
                const double slope = 1.0 - 1.0 / std::max(1.0, (double)userRatio);
                if (userKneeDb > 0.0f && under < halfKnee)
                {
                    const double xk = under / std::max(1.0, halfKnee);
                    const double soft = xk * xk;
                    gainDb = slope * under * soft;
                }
                else
                {
                    gainDb = slope * under;
                }
            }

            gainDb *= amount;
            targetGain = dbToLinear(gainDb);
            targetGain = std::clamp(targetGain, 1.0, 128.0);
        }

        // Smoothing (mirror attack/release)
        const double aAtk = timeToAlpha(timingConfig.expanderAttack);
        const double aRel = timeToAlpha(timingConfig.expanderRelease);
        if (targetGain < ch.vcaState) ch.vcaState = aAtk * targetGain + (1.0 - aAtk) * ch.vcaState;
        else                          ch.vcaState = aRel * targetGain + (1.0 - aRel) * ch.vcaState;

        float y = applyVCA(static_cast<float>(x), ch.vcaState, ch.vcaState, ch.vcaNoise, ch.noiseRng);

        // De-emphasis (LP) after expansion to restore spectral balance
        if (preDeEnabled)
        {
            double out = ch.lpYPrev + lpA * (static_cast<double>(y) - ch.lpYPrev);
            ch.lpYPrev = out;
            y = static_cast<float>(out * deGain);
        }

        return y;
    }

    // block processing API
    void processBlockCompress(float* const* io, int numChannels, int numSamples, float compAmount)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = processCompress(ch, io[ch][i], compAmount);
    }

    void processBlockExpand(float* const* io, int numChannels, int numSamples, float compAmount)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = processExpand(ch, io[ch][i], compAmount);
    }

private:
    struct ChannelState
    {
        // Sidechain envelope (RMS-style) and VCA control
        double env = 0.0;          // Linear amplitude envelope
        double envSq = 0.0;        // Squared envelope for RMS
        double vcaState = 1.0;     // Smoothed gain
        double vcaNoise = 0.0;     // VCA noise accumulator
        // Pre/de-emphasis filter state
        double hpXPrev = 0.0, hpYPrev = 0.0; // Pre-emphasis (HP)
        double lpXPrev = 0.0, lpYPrev = 0.0; // De-emphasis (LP)
        uint32_t noiseRng = 12345u; // Audio-thread-safe LCG random number generator
    };

    std::vector<ChannelState> channels;
    double currentSampleRate = 44100.0;
    
    // VCA noise floor (referenced from constants)
    static constexpr double VCA_NOISE_LEVEL = CompanderParams::VCA_NOISE_LEVEL;

    // Parameters (default values obtained from constants)
    float thresholdVrms = CompanderParams::COMP_THRESHOLD_VRMS;
    float sidechainRefVrms = CompanderParams::SIDECHAIN_REF_VRMS;
    float userRatio = CompanderParams::COMP_RATIO;
    float userKneeDb = CompanderParams::COMP_KNEE_DB;

    // Pre/de-emphasis
    bool preDeEnabled = CompanderParams::PRE_DE_EMPHASIS_ENABLED;
    double fcPreDeHz = CompanderParams::PRE_DE_EMPHASIS_FC_HZ;
    double preRcScale = CompanderParams::PRE_EMPHASIS_RC_SCALE;
    double deRcScale = CompanderParams::DE_EMPHASIS_RC_SCALE;
    double preGain = CompanderParams::PRE_EMPHASIS_MAKEUP_GAIN;
    double deGain = CompanderParams::DE_EMPHASIS_MAKEUP_GAIN;
    double hpA = 0.0; // HP coefficient（y = a*(y[n-1]+x[n]-x[n-1])）
    double lpA = 0.0; // LP coefficient（y = y[n-1] + a*(x[n]-y[n-1])）
    inline void recomputefilter() noexcept
    {
        if (currentSampleRate <= 0.0) { hpA = lpA = 0.0; return; }
        const double dt = 1.0 / currentSampleRate;
        const double baseRC = 1.0 / (2.0 * patina::compat::MathConstants<double>::pi * std::max(1.0, fcPreDeHz));
        
        const double hpRC = baseRC * preRcScale;
        hpA = hpRC / (hpRC + dt);
        
        const double lpRC = baseRC * deRcScale;
        lpA = dt / (lpRC + dt);
    }

    TimingConfig timingConfig;
    double vcaOutputRatioOverride = PartsConstants::ne570VcaOutputRatio;

    static inline double linearToDb(double v, double floorLin = 1.0e-12) noexcept
    {
        return 20.0 * std::log10(std::max(floorLin, v));
    }

    static inline double dbToLinear(double db) noexcept
    {
        return std::pow(10.0, db / 20.0);
    }

    // Helper function
    inline double timeToAlpha(double timeConstant) const noexcept
    {
        if (timeConstant <= 0.0 || currentSampleRate <= 0.0) return 1.0;
        return 1.0 - std::exp(-1.0 / (currentSampleRate * timeConstant));
    }
    // NE570-style dB-domain gain calculation (with soft knee)
    inline double computeGainDbWithKnee(double levelDb, double thresholdDb, double ratio, double kneeDb, bool isCompressor) const noexcept
    {
        const double halfKnee = kneeDb * 0.5;
        double over = levelDb - thresholdDb;

        if (isCompressor)
        {
            if (over <= -halfKnee) return 0.0;
            if (kneeDb > 0.0 && std::fabs(over) < halfKnee)
            {
                const double x = (over + halfKnee) / kneeDb;
                const double soft = x * x * (ratio - 1.0);
                return -soft * halfKnee;
            }
            if (over > 0.0)
            {
                const double slope = 1.0 - 1.0 / std::max(1.0, ratio);
                return -slope * over;
            }
            return 0.0;
        }
        else
        {
            if (over >= halfKnee) return 0.0;
            if (kneeDb > 0.0 && std::fabs(over) < halfKnee)
            {
                const double x = (halfKnee - over) / kneeDb;
                const double soft = x * x * (ratio - 1.0);
                return -soft * halfKnee;
            }
            if (over < 0.0)
            {
                const double slope = std::max(1.0, ratio) - 1.0;
                return slope * over;
            }
            return 0.0;
        }
    }
    // VCA application (linear gain specification, including nonlinear characteristics)
    inline float applyVCA(float input, double targetGainLin, double& vcaState, double& vcaNoise, uint32_t& noiseRng) const noexcept
    {
        const double alpha = timeToAlpha(0.0007);
        vcaState = alpha * targetGainLin + (1.0 - alpha) * vcaState;
        double out = static_cast<double>(input) * vcaState;

        const double sat = PartsConstants::opAmp_outputSaturation * vcaOutputRatioOverride;
        if (std::fabs(out) > sat)
        {
            const double sign = (out >= 0.0) ? 1.0 : -1.0;
            const double x = std::fabs(out) - sat;
            out = sign * (sat + x / (1.0 + x));
        }

        const double noise = VCA_NOISE_LEVEL * (2.0 - std::clamp(vcaState * 2.0, 0.0, 2.0));
        noiseRng = noiseRng * 1664525u + 1013904223u;
        const double rval = static_cast<double>(noiseRng >> 8) / 16777216.0 - 0.5;
        vcaNoise = 0.99 * vcaNoise + 0.01 * rval * noise;
        out += vcaNoise;

        return static_cast<float>(std::clamp(out, -1.5, 1.5));
    }
};
