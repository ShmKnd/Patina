#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/engine/BbdDelayEngine.h"

using namespace Catch::Matchers;

// ============================================================================
// BbdDelayEngine: basic test
// ============================================================================

TEST_CASE("BbdDelayEngine: prepare and reset", "[engine]")
{
    patina::BbdDelayEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);
    engine.reset();

    // Should be callable without crash
    REQUIRE(true);
}

TEST_CASE("BbdDelayEngine: silence in, silence out", "[engine]")
{
    patina::BbdDelayEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 256;
    float ch0[N] = {};
    float ch1[N] = {};
    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::BbdDelayEngine::Params params;
    params.mix = 0.5f;
    engine.processBlock(inputs, outputs, 2, N, params);

    // Silence input should produce near-silence output (small noise is OK)
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
        REQUIRE(std::fabs(out0[i]) < 0.01f);
        REQUIRE(std::fabs(out1[i]) < 0.01f);
    }
}

TEST_CASE("BbdDelayEngine: impulse produces delayed output", "[engine]")
{
    patina::BbdDelayEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    patina::BbdDelayEngine::Params params;
    params.delayMs = 10.0f; // 10ms delay = ~441 samples
    params.feedback = 0.0f;
    params.mix = 1.0f; // wet only
    params.compAmount = 0.0f; // no compander
    params.chorusDepth = 0.0f;
    params.emulateBBD = false;
    params.emulateOpAmpSat = false;

    // Send impulse then silence
    constexpr int N = 1024;
    float input[N] = {};
    input[0] = 0.5f;
    float output[N] = {};

    const float* ins[] = {input};
    float* outs[] = {output};
    engine.processBlock(ins, outs, 1, N, params);

    // Should get some energy in the delayed region (around sample 441)
    // Even with filtering, there should be output after the delay
    float earlyEnergy = 0.0f;
    float delayedEnergy = 0.0f;
    int delayStart = 300;
    int delayEnd = 600;

    for (int i = 1; i < delayStart; ++i)
        earlyEnergy += output[i] * output[i];
    for (int i = delayStart; i < delayEnd; ++i)
        delayedEnergy += output[i] * output[i];

    // Delayed region should have more energy than early region
    REQUIRE(delayedEnergy > earlyEnergy);
}

TEST_CASE("BbdDelayEngine: feedback creates repeats", "[engine]")
{
    patina::BbdDelayEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    patina::BbdDelayEngine::Params params;
    params.delayMs = 5.0f; // 5ms = ~220 samples
    params.feedback = 0.6f;
    params.mix = 1.0f;
    params.compAmount = 0.0f;
    params.chorusDepth = 0.0f;

    constexpr int N = 2048;
    float input[N] = {};
    input[0] = 0.5f;
    float output[N] = {};

    const float* ins[] = {input};
    float* outs[] = {output};
    engine.processBlock(ins, outs, 1, N, params);

    // With feedback, there should be energy in multiple delay periods
    float energy1 = 0.0f, energy2 = 0.0f;
    for (int i = 150; i < 400; ++i)
        energy1 += output[i] * output[i];
    for (int i = 400; i < 700; ++i)
        energy2 += output[i] * output[i];

    // Both periods should have some energy (repeats)
    REQUIRE(energy1 > 1e-10f);
    REQUIRE(energy2 > 1e-10f);
}

TEST_CASE("BbdDelayEngine: dry/wet mix", "[engine]")
{
    patina::BbdDelayEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 64;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f;

    // Fully dry
    {
        patina::BbdDelayEngine::Params params;
        params.mix = 0.0f;
        float output[N] = {};
        const float* ins[] = {input};
        float* outs[] = {output};
        engine.reset();
        engine.processBlock(ins, outs, 1, N, params);

        // Dry only - output should closely track input
        for (int i = 0; i < N; ++i)
            REQUIRE_THAT(output[i], WithinAbs(input[i], 0.01f));
    }
}

TEST_CASE("BbdDelayEngine: applyModdingConfig", "[engine]")
{
    patina::BbdDelayEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    ModdingConfig mod;
    mod.opAmp = ModdingConfig::JRC4558D;
    mod.compander = ModdingConfig::SA571N;
    mod.capGrade = ModdingConfig::Film;
    engine.applyModdingConfig(mod);

    // Process shouldn't crash after config change
    constexpr int N = 64;
    float ch0[N] = {}, ch1[N] = {};
    for (int i = 0; i < N; ++i)
        ch0[i] = ch1[i] = 0.2f * std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);

    float out0[N] = {}, out1[N] = {};
    const float* ins[] = {ch0, ch1};
    float* outs[] = {out0, out1};

    patina::BbdDelayEngine::Params params;
    engine.processBlock(ins, outs, 2, N, params);

    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
    }
}

TEST_CASE("BbdDelayEngine: in-place processing (input==output)", "[engine]")
{
    patina::BbdDelayEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 128;
    float buf[N];
    for (int i = 0; i < N; ++i)
        buf[i] = 0.3f * std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);

    const float* ins[] = {buf};
    float* outs[] = {buf}; // same buffer
    patina::BbdDelayEngine::Params params;
    params.mix = 0.5f;

    // Should not crash or produce NaN with in-place
    engine.processBlock(ins, outs, 1, N, params);
    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}

TEST_CASE("BbdDelayEngine: accessor methods", "[engine]")
{
    patina::BbdDelayEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    // Const accessors should work
    [[maybe_unused]] const auto& ib = engine.getInputBuffer();
    [[maybe_unused]] const auto& il = engine.getInputFilter();
    [[maybe_unused]] const auto& comp = engine.getCompressor();
    [[maybe_unused]] const auto& exp = engine.getExpander();
    [[maybe_unused]] const auto& bbd = engine.getBbdStageEmu();
    [[maybe_unused]] const auto& tf = engine.getToneFilter();
    [[maybe_unused]] const auto& fb = engine.getFeedbackMod();
    [[maybe_unused]] const auto& om = engine.getOutputMod();
    REQUIRE(true);
}
