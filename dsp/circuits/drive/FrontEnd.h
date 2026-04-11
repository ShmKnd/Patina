// Restored old working version

// Performance optimization: Disable debug logging in release builds
// Standalone: debug logs removed at compile time
#define ANALOG_DELAY_DBG(x) ((void)0)  // Disabled in standalone build

#pragma once
#include <atomic>
#include "InputBuffer.h"
#include "InputFilter.h"
#include "../../constants/PartsConstants.h"
#include "../compander/CompanderModule.h"
#include "../../core/AudioCompat.h"

struct FrontEnd {
    static inline void processFrame(
        const float* const* inFS,
        int numBufferChannels,
        const int* inMap,
        int numChannels,
        float fsToVGain,
        bool isInstrument,
        InputBuffer& tl072,
        InputFilter& lpf,
        float* outDryV,
        float* outWetFrontV)
    {
        for (int ch = 0; ch < numChannels; ++ch) {
            const int inputCh = inMap[ch];
            const float* rp = (inputCh < numBufferChannels ? inFS[inputCh] : nullptr);
            const float in = rp ? rp[0] : 0.0f;
            patina::compat::ignoreUnused(in);
            outDryV[ch] = 0.0f;
            outWetFrontV[ch] = 0.0f;
        }
    }

    static inline void processOne(
        float inFS,
        float fsToVGain,
        bool isInstrument,
        int ch,
        InputBuffer& tl072,
        InputFilter& lpf,
        float& outDryV,
        float& outWetFrontV)
    {
        const float padGain = isInstrument ? static_cast<float>(PartsConstants::padGainInstrument) : static_cast<float>(PartsConstants::padGainLine);
        float wetIn = inFS * fsToVGain;
        wetIn *= padGain;
        outDryV = wetIn;
        float afterTL072 = tl072.process(ch, wetIn);
        outWetFrontV = lpf.process(ch, afterTL072);
    }

    // Stereo-specific SIMD-optimized (2-channel simultaneous processing)
    static inline void processStereoReorderedSIMD(
        float inFS_L, float inFS_R,
        float fsToVGain,
        bool isInstrument,
        InputBuffer& tl072,
        InputFilter& lpf,
        CompanderModule* companderPre,
        bool companderEnabled,
        float companderAmount,
        float& inputLevelSmoothed,
        float inputLevelAlpha,
        float inputLevelTarget,
        float& outDryV_L, float& outDryV_R,
        float& outWetFrontV_L, float& outWetFrontV_R,
        float& outCompressGain_L, float& outCompressGain_R)
    {
        const float padGain = isInstrument ? static_cast<float>(PartsConstants::padGainInstrument) : static_cast<float>(PartsConstants::padGainLine);
        
        // L/R simultaneous processing
        float wetIn_L = inFS_L * fsToVGain * padGain;
        float wetIn_R = inFS_R * fsToVGain * padGain;
        
        inputLevelSmoothed += inputLevelAlpha * (inputLevelTarget - inputLevelSmoothed);
        wetIn_L *= inputLevelSmoothed;
        wetIn_R *= inputLevelSmoothed;
        
        outDryV_L = wetIn_L;
        outDryV_R = wetIn_R;
        outCompressGain_L = 1.0f;
        outCompressGain_R = 1.0f;
        
        // TL072 processing (stereo)
        float afterTL072_L = tl072.process(0, wetIn_L);
        float afterTL072_R = tl072.process(1, wetIn_R);
        
        // InputFilter (SIMD parallel processing)
        lpf.processStereo(afterTL072_L, afterTL072_R);
        
        // Compressor processing
        float afterComp_L = afterTL072_L;
        float afterComp_R = afterTL072_R;
        if (companderPre && companderEnabled && companderAmount > 0.0f) {
            afterComp_L = companderPre->processCompress(0, afterTL072_L, companderAmount, &outCompressGain_L);
            afterComp_R = companderPre->processCompress(1, afterTL072_R, companderAmount, &outCompressGain_R);
        }
        
        outWetFrontV_L = afterComp_L;
        outWetFrontV_R = afterComp_R;
    }

    static inline void processOneReordered(
        float inFS,
        float fsToVGain,
        bool isInstrument,
        int ch,
    InputBuffer& tl072,
    InputFilter& lpf,
    CompanderModule* companderPre,
    bool companderEnabled,
    float companderAmount,
        float& inputLevelSmoothed,
        float inputLevelAlpha,
        float inputLevelTarget,
        float& outDryV,
        float& outWetFrontV,
        float& outCompressGain)
    {
        const float padGain = isInstrument ? static_cast<float>(PartsConstants::padGainInstrument) : static_cast<float>(PartsConstants::padGainLine);
        float wetIn = inFS * fsToVGain;
        wetIn *= padGain;
        inputLevelSmoothed += inputLevelAlpha * (inputLevelTarget - inputLevelSmoothed);
        wetIn *= inputLevelSmoothed;
        // Rate-limited diagnostic logging to inspect the voltage passed to the TL072
        // without spamming the console. Logs ~1/1024th of samples.
        // Bug fix: bitwise wraparound (int overflow prevention)
        static std::atomic<int> feDbgCounter{0};
        int c = feDbgCounter.fetch_add(1, std::memory_order_relaxed) & 0x7FFFFFFF; // Limit to positive range
        if ((c & 1023) == 0)
        {
            ANALOG_DELAY_DBG(patina::compat::String("[FrontEnd][DIAG] ch=")
                + patina::compat::String(std::to_string(ch).c_str())
                + patina::compat::String(" inFS=") + patina::compat::String(std::to_string(inFS).c_str())
                + patina::compat::String(" fsToV=") + patina::compat::String(std::to_string(fsToVGain).c_str())
                + patina::compat::String(" padGain=") + patina::compat::String(std::to_string(padGain).c_str())
                + patina::compat::String(" inputLevelSmoothed=") + patina::compat::String(std::to_string(inputLevelSmoothed).c_str())
                + patina::compat::String(" wetIn=") + patina::compat::String(std::to_string(wetIn).c_str()));
        }
        // Dry signal before compressor (original spec)
        outDryV = wetIn;
        outCompressGain = 1.0f;
        // New path: TL072 → InputFilter → Compressor → BBD
        float afterTL072 = tl072.process(ch, wetIn);
        float afterLPF = lpf.process(ch, afterTL072);
        float afterComp = afterLPF;
        if (companderPre && companderEnabled && companderAmount > 0.0f)
        {
            afterComp = companderPre->processCompress(ch, afterLPF, companderAmount, &outCompressGain);
            // Debug: log only first few samples (overflow-safe)
            static std::atomic<int> feDbgCnt{0};
            int cnt = feDbgCnt.fetch_add(1, std::memory_order_relaxed);
            if (cnt < 5 && ch == 0)
            {
                std::fprintf(stderr, "[FrontEnd] wetIn=%.6f afterLPF=%.6f afterComp=%.6f gain=%.6f\n", 
                            wetIn, afterLPF, afterComp, outCompressGain);
                std::fflush(stderr);
            }
        }
        outWetFrontV = afterComp;
    }

    // Overload: version accepting dummyBypass parameters (for backward compatibility)
    static inline void processOneReordered(
        float inFS,
        float fsToVGain,
        bool isInstrument,
        int ch,
        InputBuffer& tl072,
        InputFilter& lpf,
        CompanderModule* companderPre,
        bool companderEnabled,
        float companderAmount,
        float& inputLevelSmoothed,
        float inputLevelAlpha,
        float inputLevelTarget,
        bool /*dummyBypass*/,
        float& outDryV,
        float& outWetFrontV,
        float& outCompressGain)
    {
        // Ignore dummyBypass and call main implementation
        processOneReordered(inFS, fsToVGain, isInstrument, ch, tl072, lpf, 
                           companderPre, companderEnabled, companderAmount,
                           inputLevelSmoothed, inputLevelAlpha, inputLevelTarget,
                           outDryV, outWetFrontV, outCompressGain);
    }
};
