/*
    example_drive_engine.cpp — DriveEngine sample
    ============================================================
    Pedal-style overdrive/distortion
    Signal path: InputBuffer(TL072) → DiodeClipper → ToneFilter → OutputStage → Dry/Wet

    This sample switches between Si / Schottky / Ge diode types and writes
    each tonal variation into one WAV file.

    Build:
        c++ -std=c++17 -O2 -I.. example_drive_engine.cpp -o example_drive
*/

#include "include/patina.h"
#include "examples/wav_writer.h"

#include <cstdio>
#include <vector>

int main()
{
    constexpr int    kSampleRate   = 48000;
    constexpr int    kBlockSize    = 256;
    constexpr int    kChannels     = 2;
    constexpr float  kDurationSec  = 3.0f;  // 3 diode types × 1 second each
    constexpr int    kTotalSamples = (int)(kSampleRate * kDurationSec);

    // ----- Engine initialization -----
    patina::DriveEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    // ----- Test signal (440 Hz sine burst) -----
    auto testSignal = wav::sineBurst(kSampleRate, 440.0f, kDurationSec, 0.6f, 30.0f, 300.0f);

    // ----- Output buffer -----
    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    // ----- 3 segments: Si → Schottky → Ge -----
    const int segmentLen = kTotalSamples / 3;
    const char* diodeNames[] = {"Si (Silicon)", "Schottky", "Ge (Germanium)"};

    for (int seg = 0; seg < 3; ++seg)
    {
        patina::DriveEngine::Params params;
        params.drive        = 0.75f;
        params.clippingMode = 1;          // Diode
        params.diodeType    = seg;        // 0=Si, 1=Schottky, 2=Ge
        params.tone         = 0.6f;
        params.outputLevel  = 0.7f;
        params.mix          = 1.0f;
        params.supplyVoltage = 9.0;

        // Enable power sag to emulate reduced headroom under strong input
        params.enablePowerSag = true;
        params.sagAmount      = 0.4f;

        std::printf("  Section %d: %s diode clipping\n", seg + 1, diodeNames[seg]);

        const int start = seg * segmentLen;
        const int end   = (seg == 2) ? kTotalSamples : start + segmentLen;

        // Process in blocks
        for (int pos = start; pos < end; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, end - pos);

            // Input pointers (mono signal duplicated to both channels)
            const float* inPtrs[2] = { &testSignal[(size_t)pos],
                                       &testSignal[(size_t)pos] };
            float* outPtrs[2] = { &output[0][(size_t)pos],
                                  &output[1][(size_t)pos] };

            engine.processBlock(inPtrs, outPtrs, kChannels, n, params);
        }
    }

    // ----- WAV output -----
    if (wav::write("output_drive.wav", output, kSampleRate))
        std::printf("=> output_drive.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);
    else
        std::fprintf(stderr, "Error: failed to write WAV\n");

    return 0;
}
