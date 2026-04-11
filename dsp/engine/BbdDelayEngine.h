#pragma once

#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include "../core/ProcessSpec.h"
#include "../core/AudioCompat.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../constants/PartsConstants.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/InputFilter.h"
#include "../circuits/drive/OutputStage.h"
#include "../circuits/modulation/AnalogLfo.h"
#include "../config/ModdingConfig.h"
#include "../circuits/compander/CompanderModule.h"
#include "../circuits/bbd/BbdSampler.h"
#include "../circuits/bbd/BbdStageEmulator.h"
#include "../circuits/bbd/BbdClock.h"
#include "../circuits/bbd/BbdFeedback.h"
#include "../circuits/filters/ToneFilter.h"
#include "../circuits/mixer/Mixer.h"
#include "../config/Presets.h"

namespace patina {

// BBD analog delay engine — full chain integrated into one class
// Internal signal path:
//   Outboard: Input → CompanderCompress → DelayLine → BbdSampler(S&H) → BbdStageEmulator
//             → CompanderExpand → ToneFilter → Mixer(dry/wet)
//   Pedal:    Input → InputBuffer → InputFilter → CompanderCompress
//             → DelayLine → BbdSampler(S&H) → BbdStageEmulator
//             → CompanderExpand → ToneFilter → OutputStage → Mixer(dry/wet)
//   pedalMode=false (default) for outboard quality, true for pedal quality
//   (BbdFeedback → feedback return to delay write point)
class BbdDelayEngine
{
public:
    struct Params
    {
        float delayMs       = 250.0f;
        float feedback      = 0.3f;
        float tone          = 0.5f;
        float mix           = 0.5f;
        float compAmount    = 0.5f;
        float chorusDepth   = 0.0f;
        float lfoRateHz     = 0.5f;

        double supplyVoltage = 9.0;
        int    bbdStages     = PartsConstants::bbdStagesDefault;

        bool emulateBBD        = true;
        bool emulateOpAmpSat   = true;
        bool emulateToneRC     = true;
        bool enableAging       = false;
        double ageYears        = 0.0;
        double capacitanceScale = 1.0;

        bool  pedalMode        = false;   // true=pedal quality(InputBuffer+InputFilter+OutputStage), false=outboard quality
    };

    BbdDelayEngine() = default;

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;
        const int nCh  = spec.numChannels;
        const double sr = spec.sampleRate;

        tl072.prepare(spec);
        inputLpf.prepare(spec);
        compressor.prepare(spec);
        expander.prepare(spec);
        bbdSampler.prepare(spec);
        bbdStageEmu.prepare(spec);
        lfo.prepare(spec);
        toneFilter.prepare(spec);
        feedbackMod.prepare(spec);
        outputMod.prepare(spec);

        // Allocate delay buffer (up to 2 seconds)
        maxDelaySamples = static_cast<int>(sr * 2.0) + 1;
        delayBuffer.resize((size_t)nCh);
        for (auto& ch : delayBuffer)
            ch.assign((size_t)maxDelaySamples, 0.0f);

        writePos = 0;
        fbSamples.assign((size_t)nCh, 0.0f);
    }

    void reset()
    {
        tl072.reset();
        inputLpf.reset();
        compressor.reset();
        expander.reset();
        bbdSampler.reset();
        bbdStageEmu.reset();
        lfo.reset();
        toneFilter.reset();
        feedbackMod.reset();
        outputMod.reset();

        for (auto& ch : delayBuffer)
            std::fill(ch.begin(), ch.end(), 0.0f);
        std::fill(fbSamples.begin(), fbSamples.end(), 0.0f);
        writePos = 0;
    }

    // Compander settings
    void setCompressorConfig(const CompanderModule::Config& cfg) { compressor.setConfig(cfg); }
    void setExpanderConfig(const CompanderModule::Config& cfg) { expander.setConfig(cfg); }

    // Op-amp settings
    void setOpAmpOverrides(const BbdFeedback::OpAmpOverrides& ovr) { feedbackMod.setOpAmpOverrides(ovr); }

    // BBD stage emulator bandwidth scale
    void setBandwidthScale(double s) { bbdStageEmu.setBandwidthScale(s); }

    // Output LPF cutoff
    void setOutputCutoffHz(double fc) { outputMod.setCutoffHz(fc); }

    // Batch configure from ModdingConfig
    void applyModdingConfig(const ModdingConfig& mod)
    {
        // Op-amp
        feedbackMod.setOpAmpOverrides(presets::opAmpFromType(mod.opAmp));

        // Compander
        CompanderModule::Config compCfg;
        if (mod.compander == ModdingConfig::SA571N)
            compCfg = presets::sa571n();
        else
            compCfg = presets::ne570();
        compressor.setConfig(compCfg);
        expander.setConfig(compCfg);

        // Capacitor grade
        bbdStageEmu.setBandwidthScale(presets::capGradeBandwidthScale(mod.capGrade));
    }

    // Block processing: in-place (input == output allowed)
    void processBlock(const float* const* input, float* const* output,
                      int numChannels, int numSamples, const Params& params)
    {
        const double sr = currentSpec.sampleRate;
        const int nCh = std::min(numChannels, (int)delayBuffer.size());
        if (nCh <= 0 || numSamples <= 0) return;
        ScopedDenormalDisable denormalGuard;

        // Parameter safety clamp
        const double safeSupplyV = std::clamp(params.supplyVoltage, 1.0, 48.0);
        const float  safeFeedback = std::clamp(params.feedback, 0.0f, 0.98f);

        // LFO rate update
        lfo.setRateHz(params.lfoRateHz);
        lfo.setSupplyVoltage(safeSupplyV);

        // Update tone filter
        toneFilter.updateToneFilterIfNeeded(params.tone, params.capacitanceScale, params.emulateToneRC);

        // OutputStage cutoff (scaled by supply voltage)
        outputMod.setCutoffHz(PartsConstants::outputLpfFcHz);

        // BBD step calculation
        const double delaySamples = std::max(1.5, (double)params.delayMs * 0.001 * sr);
        const double baseStep = std::max(1.0, delaySamples / std::max(1.0, (double)params.bbdStages));

        // Chorus depth
        const double chorusDepthSmp = BbdClock::chorusDepthSamples(
            params.chorusDepth, params.delayMs, sr, 5.0);

        // bbdFrame: reuse member variable (avoid heap allocation per block)
        if ((int)bbdFrameVec.size() < nCh) bbdFrameVec.resize((size_t)nCh);

        for (int i = 0; i < numSamples; ++i)
        {
            lfo.stepAll();

            for (int ch = 0; ch < nCh; ++ch)
            {
                float dry = FastMath::sanitize(input[ch][i]);

                // ===== Input stage (pedal mode only) =====
                float wet = dry;
                if (params.pedalMode) {
                    wet = tl072.process(ch, dry);
                    wet = inputLpf.process(ch, wet);
                }

                // ===== Compressor =====
                if (params.compAmount > 0.0f)
                    wet = compressor.processCompress(ch, wet, params.compAmount);

                // ===== Delay line write (with feedback mixing)=====
                float toDelay = wet + fbSamples[(size_t)ch] * safeFeedback;
                delayBuffer[(size_t)ch][(size_t)writePos] = toDelay;

                // ===== Delay line read (with chorus modulation)=====
                double sinV = lfo.getSinLike(ch);
                double effectiveDelay = BbdClock::effectiveDelaySamples(
                    delaySamples, chorusDepthSmp, sinV);

                double readPos = (double)writePos - effectiveDelay;
                while (readPos < 0.0) readPos += maxDelaySamples;
                int rp0 = (int)readPos;
                double frac = readPos - rp0;
                int rp1 = (rp0 + 1) % maxDelaySamples;
                float delayed = (float)((1.0 - frac) * delayBuffer[(size_t)ch][(size_t)rp0]
                                       + frac * delayBuffer[(size_t)ch][(size_t)rp1]);

                // ===== BBD sampler (S&H) =====
                double triV = lfo.getTri(ch);
                double stepEff = BbdClock::stepWithClock(baseStep, triV, params.chorusDepth, 0.15);
                delayed = bbdSampler.processSample(ch, delayed, stepEff,
                                                   params.emulateBBD, params.enableAging, 0.0,
                                                   rng, normalDist);

                bbdFrameVec[(size_t)ch] = delayed;
            }

            // ===== BBD stage emulator (all channels simultaneously) =====
            bbdStageEmu.process(bbdFrameVec, baseStep, params.bbdStages,
                                safeSupplyV, params.enableAging, params.ageYears);

            for (int ch = 0; ch < nCh; ++ch)
            {
                float wetOut = bbdFrameVec[(size_t)ch];

                // ===== Expander =====
                if (params.compAmount > 0.0f)
                    wetOut = expander.processExpand(ch, wetOut, params.compAmount);

                // ===== tonefilter =====
                wetOut = toneFilter.processSample(ch, wetOut);

                // ===== Output stage (pedal mode only) =====
                if (params.pedalMode)
                    wetOut = outputMod.process(ch, wetOut, safeSupplyV);

                // ===== Feedback path =====
                fbSamples[(size_t)ch] = feedbackMod.process(
                    ch, wetOut,
                    params.emulateOpAmpSat,
                    rng, normalDist,
                    1.0, // opAmpNoiseGain
                    PartsConstants::bbdBaseNoise,
                    currentSpec.sampleRate,
                    safeSupplyV > 12.0,
                    params.capacitanceScale);

                // ===== Dry/Wet Mix =====
                float dryGain, wetGain;
                Mixer::equalPowerGains(params.mix, dryGain, wetGain);
                output[ch][i] = FastMath::sanitize(input[ch][i]) * dryGain + wetOut * wetGain;
            }

            writePos = (writePos + 1) % maxDelaySamples;
        }

        // zero-clear surplus channels
        for (int ch = nCh; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                output[ch][i] = 0.0f;
    }

    // read-only access to internal modules (for testing/diagnostics)
    const InputBuffer&   getInputBuffer()   const { return tl072; }
    const InputFilter&           getInputFilter()      const { return inputLpf; }
    const CompanderModule&    getCompressor()    const { return compressor; }
    const CompanderModule&    getExpander()      const { return expander; }
    const BbdStageEmulator&   getBbdStageEmu()   const { return bbdStageEmu; }
    const ToneFilter&     getToneFilter()    const { return toneFilter; }
    const BbdFeedback&     getFeedbackMod()   const { return feedbackMod; }
    const OutputStage&       getOutputMod()     const { return outputMod; }

private:
    ProcessSpec currentSpec;

    InputBuffer  tl072;
    InputFilter          inputLpf;
    CompanderModule   compressor;
    CompanderModule   expander;
    BbdSampler        bbdSampler;
    BbdStageEmulator  bbdStageEmu;
    AnalogLfo          lfo;
    ToneFilter    toneFilter;
    BbdFeedback    feedbackMod;
    OutputStage      outputMod;

    // Delay buffer: [channel][sample]
    std::vector<std::vector<float>> delayBuffer;
    int maxDelaySamples = 0;
    int writePos = 0;

    // Feedback return value
    std::vector<float> fbSamples;

    // bbdFrame working buffer (reused within processBlock)
    std::vector<float> bbdFrameVec;

    // Internal RNG (for noise generation in BbdFeedback/BbdSampler)
    std::minstd_rand rng{42};
    std::normal_distribution<double> normalDist{0.0, 1.0};
};

} // namespace patina
