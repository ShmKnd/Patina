/*
    example_filter_engine.cpp — FilterEngine sample
    ============================================================
    Dual-filter + triple-drive effect
    Signal path:
        Serial:   Drive1 → Filter1 → Drive2 → Filter2 → Drive3
        Parallel: Drive1 → [Filter1 | Filter2] → Drive3

    Eight sections:
        1. Serial LPF+HPF (Tube drive)           — default -12dB slope
        2. Serial BPF+LPF (Diode drive)          — default -12dB slope
        3. Parallel LPF+HPF (WaveFolder drive)   — default -12dB slope
        4. Parallel BPF+BPF (Tape drive)         — default -12dB slope
        5. Serial Ladder (Tube drive)            — Ladder type (-24dB fixed)
        6. Serial -6dB LPF → -24dB HPF (Diode)  — slope-switch demo
        7. Serial -18dB BPF → -6dB LPF (Tape)   — slope-switch demo
        8. Parallel -24dB LPF | -24dB HPF (Wave) — steep filter + gain compensation demo

    Build:
        c++ -std=c++17 -O2 -I.. example_filter_engine.cpp -o example_filter
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
    constexpr float kSegmentSec   = 2.0f;
    constexpr int   kSegments     = 8;
    constexpr float kDurationSec  = kSegmentSec * kSegments;
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);

    // ----- Engine initialization -----
    patina::FilterEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    // ----- Test signal (440 Hz sine burst) -----
    auto testSignal = wav::sineBurst(kSampleRate, 440.0f, kDurationSec, 0.6f, 30.0f, 300.0f);

    // ----- Output buffer -----
    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    // ----- 8 sections -----
    const int segLen = (int)(kSampleRate * kSegmentSec);

    struct Section {
        const char* name;
        patina::FilterEngine::Params params;
    };

    Section sections[kSegments];

    // Section 1: Serial — LPF → HPF, Tube drive (default -12dB)
    {
        auto& p = sections[0].params;
        sections[0].name = "Serial: LPF+HPF, Tube [-12dB]";
        p.routing = patina::FilterEngine::Serial;
        p.filter1CutoffHz = 800.0f;    p.filter1Resonance = 0.6f;
        p.filter1Type = patina::FilterEngine::LowPass;
        p.filter1Slope = patina::FilterEngine::Slope_12dB;
        p.filter2CutoffHz = 300.0f;    p.filter2Resonance = 0.4f;
        p.filter2Type = patina::FilterEngine::HighPass;
        p.filter2Slope = patina::FilterEngine::Slope_12dB;
        p.drive1Amount = 0.4f; p.drive1Type = patina::FilterEngine::Tube;
        p.drive2Amount = 0.3f; p.drive2Type = patina::FilterEngine::Tube;
        p.drive3Amount = 0.2f; p.drive3Type = patina::FilterEngine::Tube;
        p.outputLevel = 0.7f;  p.mix = 1.0f;
    }

    // Section 2: Serial — BPF → LPF, Diode drive (default -12dB)
    {
        auto& p = sections[1].params;
        sections[1].name = "Serial: BPF+LPF, Diode [-12dB]";
        p.routing = patina::FilterEngine::Serial;
        p.filter1CutoffHz = 1200.0f;   p.filter1Resonance = 0.7f;
        p.filter1Type = patina::FilterEngine::BandPass;
        p.filter1Slope = patina::FilterEngine::Slope_12dB;
        p.filter2CutoffHz = 2000.0f;   p.filter2Resonance = 0.3f;
        p.filter2Type = patina::FilterEngine::LowPass;
        p.filter2Slope = patina::FilterEngine::Slope_12dB;
        p.drive1Amount = 0.5f; p.drive1Type = patina::FilterEngine::Diode;
        p.drive2Amount = 0.4f; p.drive2Type = patina::FilterEngine::Diode;
        p.drive3Amount = 0.3f; p.drive3Type = patina::FilterEngine::Diode;
        p.outputLevel = 0.7f;  p.mix = 1.0f;
    }

    // Section 3: Parallel — LPF | HPF, WaveFolder drive (default -12dB)
    {
        auto& p = sections[2].params;
        sections[2].name = "Parallel: LPF|HPF, WaveFolder [-12dB]";
        p.routing = patina::FilterEngine::Parallel;
        p.filter1CutoffHz = 600.0f;    p.filter1Resonance = 0.5f;
        p.filter1Type = patina::FilterEngine::LowPass;
        p.filter1Slope = patina::FilterEngine::Slope_12dB;
        p.filter2CutoffHz = 2000.0f;   p.filter2Resonance = 0.5f;
        p.filter2Type = patina::FilterEngine::HighPass;
        p.filter2Slope = patina::FilterEngine::Slope_12dB;
        p.drive1Amount = 0.5f; p.drive1Type = patina::FilterEngine::Wave;
        p.drive3Amount = 0.3f; p.drive3Type = patina::FilterEngine::Wave;
        p.outputLevel = 0.7f;  p.mix = 1.0f;
    }

    // Section 4: Parallel — BPF | BPF, Tape drive (default -12dB)
    {
        auto& p = sections[3].params;
        sections[3].name = "Parallel: BPF|BPF, Tape [-12dB]";
        p.routing = patina::FilterEngine::Parallel;
        p.filter1CutoffHz = 500.0f;    p.filter1Resonance = 0.8f;
        p.filter1Type = patina::FilterEngine::BandPass;
        p.filter1Slope = patina::FilterEngine::Slope_12dB;
        p.filter2CutoffHz = 3000.0f;   p.filter2Resonance = 0.8f;
        p.filter2Type = patina::FilterEngine::BandPass;
        p.filter2Slope = patina::FilterEngine::Slope_12dB;
        p.drive1Amount = 0.6f; p.drive1Type = patina::FilterEngine::Tape;
        p.drive3Amount = 0.4f; p.drive3Type = patina::FilterEngine::Tape;
        p.outputLevel = 0.7f;  p.mix = 1.0f;
    }

    // Section 5: Serial — Ladder type (Tube drive)
    {
        auto& p = sections[4].params;
        sections[4].name = "Serial: Ladder+Ladder, Tube";
        p.routing = patina::FilterEngine::Serial;
        p.filter1CutoffHz = 1000.0f;   p.filter1Resonance = 0.7f;
        p.filter1Type = patina::FilterEngine::Ladder;     // ← Ladder
        p.filter1Slope = patina::FilterEngine::Slope_24dB; // Ladder は -24dB 固定
        p.filter2CutoffHz = 1500.0f;   p.filter2Resonance = 0.5f;
        p.filter2Type = patina::FilterEngine::Ladder;
        p.filter2Slope = patina::FilterEngine::Slope_24dB;
        p.drive1Amount = 0.3f; p.drive1Type = patina::FilterEngine::Tube;
        p.drive2Amount = 0.2f; p.drive2Type = patina::FilterEngine::Tube;
        p.drive3Amount = 0.2f; p.drive3Type = patina::FilterEngine::Tube;
        p.outputLevel = 0.7f;  p.mix = 1.0f;
    }

    // Section 6: Serial — -6dB LPF → -24dB HPF, Diode (slope-switch)
    {
        auto& p = sections[5].params;
        sections[5].name = "Serial: LPF[-6dB] -> HPF[-24dB], Diode";
        p.routing = patina::FilterEngine::Serial;
        p.filter1CutoffHz = 2000.0f;   p.filter1Resonance = 0.3f;
        p.filter1Type = patina::FilterEngine::LowPass;
        p.filter1Slope = patina::FilterEngine::Slope_6dB;   // ← -6dB
        p.filter2CutoffHz = 200.0f;    p.filter2Resonance = 0.4f;
        p.filter2Type = patina::FilterEngine::HighPass;
        p.filter2Slope = patina::FilterEngine::Slope_24dB;  // ← -24dB
        p.drive1Amount = 0.4f; p.drive1Type = patina::FilterEngine::Diode;
        p.drive2Amount = 0.3f; p.drive2Type = patina::FilterEngine::Diode;
        p.drive3Amount = 0.2f; p.drive3Type = patina::FilterEngine::Diode;
        p.outputLevel = 0.7f;  p.mix = 1.0f;
    }

    // Section 7: Serial — -18dB BPF → -6dB LPF, Tape (slope-switch)
    {
        auto& p = sections[6].params;
        sections[6].name = "Serial: BPF[-18dB] -> LPF[-6dB], Tape";
        p.routing = patina::FilterEngine::Serial;
        p.filter1CutoffHz = 800.0f;    p.filter1Resonance = 0.6f;
        p.filter1Type = patina::FilterEngine::BandPass;
        p.filter1Slope = patina::FilterEngine::Slope_18dB;  // ← -18dB
        p.filter2CutoffHz = 4000.0f;   p.filter2Resonance = 0.2f;
        p.filter2Type = patina::FilterEngine::LowPass;
        p.filter2Slope = patina::FilterEngine::Slope_6dB;   // ← -6dB
        p.drive1Amount = 0.5f; p.drive1Type = patina::FilterEngine::Tape;
        p.drive2Amount = 0.3f; p.drive2Type = patina::FilterEngine::Tape;
        p.drive3Amount = 0.2f; p.drive3Type = patina::FilterEngine::Tape;
        p.outputLevel = 0.7f;  p.mix = 1.0f;
    }

    // Section 8: Parallel — -24dB LPF | -24dB HPF, WaveFolder (gain-compensation demo)
    {
        auto& p = sections[7].params;
        sections[7].name = "Parallel: LPF[-24dB]|HPF[-24dB], Wave (comp.)";
        p.routing = patina::FilterEngine::Parallel;
        p.filter1CutoffHz = 400.0f;    p.filter1Resonance = 0.5f;
        p.filter1Type = patina::FilterEngine::LowPass;
        p.filter1Slope = patina::FilterEngine::Slope_24dB;  // ← -24dB 急峻
        p.filter2CutoffHz = 4000.0f;   p.filter2Resonance = 0.5f;
        p.filter2Type = patina::FilterEngine::HighPass;
        p.filter2Slope = patina::FilterEngine::Slope_24dB;
        p.drive1Amount = 0.4f; p.drive1Type = patina::FilterEngine::Wave;
        p.drive3Amount = 0.3f; p.drive3Type = patina::FilterEngine::Wave;
        p.outputLevel = 0.7f;  p.mix = 1.0f;
    }

    // ----- Processing -----
    for (int seg = 0; seg < kSegments; ++seg)
    {
        std::printf("  Section %d: %s\n", seg + 1, sections[seg].name);

        const int start = seg * segLen;
        const int end   = (seg == kSegments - 1) ? kTotalSamples : start + segLen;

        for (int pos = start; pos < end; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, end - pos);

            const float* inPtrs[2] = { &testSignal[(size_t)pos],
                                       &testSignal[(size_t)pos] };
            float* outPtrs[2] = { &output[0][(size_t)pos],
                                  &output[1][(size_t)pos] };

            engine.processBlock(inPtrs, outPtrs, kChannels, n, sections[seg].params);
        }
    }

    // ----- WAV output -----
    if (wav::write("output_filter.wav", output, kSampleRate))
        std::printf("  -> output_filter.wav written (%d samples, %d ch)\n",
                    kTotalSamples, kChannels);
    else
        std::printf("  !! WAV write failed\n");

    return 0;
}
