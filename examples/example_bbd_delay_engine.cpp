/*
    example_bbd_delay_engine.cpp — BbdDelayEngine sample
    ============================================================
    Full chain for an analog BBD (Bucket Brigade Device) delay:
        InputBuffer → InputFilter → Compander(compress) → DelayLine
        → BbdSampler(S&H) → BbdStageEmulator → Compander(expand)
        → ToneFilter → OutputStage → Dry/Wet

    Three scenarios:
        1. Clean slapback delay (100ms, low feedback)
        2. Warm tape-echo style (350ms, medium feedback + compander)
        3. Chorus + long delay (250ms + LFO modulation + aging)

    Build:
        c++ -std=c++17 -O2 -I.. example_bbd_delay_engine.cpp -o example_bbd
*/

#include "include/patina.h"
#include "examples/wav_writer.h"

#include <cstdio>
#include <vector>

int main()
{
    constexpr int   kSampleRate   = 48000;
    constexpr int   kBlockSize    = 256;
    constexpr int   kChannels     = 2;
    constexpr float kDurationSec  = 6.0f;
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);

    patina::BbdDelayEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    // ModdingConfig: NE570 compander + standard op-amp
    patina::ModdingConfig mod;
    mod.compander = patina::ModdingConfig::NE570;
    mod.opAmp     = patina::ModdingConfig::TL072;
    mod.capGrade  = patina::ModdingConfig::Standard;
    engine.applyModdingConfig(mod);

    auto testSignal = wav::sineBurst(kSampleRate, 440.0f, kDurationSec, 0.6f, 25.0f, 600.0f);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 3;

    struct DelaySetting {
        const char* name;
        float delayMs;
        float feedback;
        float compAmount;
        float chorusDepth;
        float lfoRateHz;
        bool  enableAging;
        double ageYears;
    };
    DelaySetting settings[] = {
        {"Slapback (100ms)",           100.0f, 0.15f, 0.0f, 0.0f, 0.5f, false, 0.0},
        {"Warm echo (350ms)",          350.0f, 0.45f, 0.6f, 0.0f, 0.5f, false, 0.0},
        {"Chorus delay (250ms, aged)", 250.0f, 0.35f, 0.4f, 0.5f, 1.5f, true,  15.0},
    };

    for (int seg = 0; seg < 3; ++seg)
    {
        engine.reset();
        auto& s = settings[seg];

        patina::BbdDelayEngine::Params params;
        params.delayMs         = s.delayMs;
        params.feedback        = s.feedback;
        params.tone            = 0.5f;
        params.mix             = 0.5f;
        params.compAmount      = s.compAmount;
        params.chorusDepth     = s.chorusDepth;
        params.lfoRateHz       = s.lfoRateHz;
        params.supplyVoltage   = 9.0;
        params.emulateBBD      = true;
        params.emulateOpAmpSat = true;
        params.emulateToneRC   = true;
        params.enableAging     = s.enableAging;
        params.ageYears        = s.ageYears;

        std::printf("  Section %d: %s\n", seg + 1, s.name);

        const int start = seg * segmentLen;
        const int end   = (seg == 2) ? kTotalSamples : start + segmentLen;

        for (int pos = start; pos < end; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, end - pos);
            const float* inPtrs[2] = { &testSignal[(size_t)pos],
                                       &testSignal[(size_t)pos] };
            float* outPtrs[2] = { &output[0][(size_t)pos],
                                  &output[1][(size_t)pos] };
            engine.processBlock(inPtrs, outPtrs, kChannels, n, params);
        }
    }

    if (wav::write("output_bbd_delay.wav", output, kSampleRate))
        std::printf("=> output_bbd_delay.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);

    return 0;
}
