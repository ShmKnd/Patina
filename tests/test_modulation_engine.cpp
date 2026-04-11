#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/engine/ModulationEngine.h"

using namespace Catch::Matchers;

// ============================================================================
// ModulationEngine: basic test
// ============================================================================

TEST_CASE("ModulationEngine: prepare and reset", "[engine][modulation]")
{
    patina::ModulationEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);
    engine.reset();
    REQUIRE(true);
}

TEST_CASE("ModulationEngine: silence in, silence out", "[engine][modulation]")
{
    patina::ModulationEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 256;
    float ch0[N] = {};
    float ch1[N] = {};
    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::ModulationEngine::Params params;
    engine.processBlock(inputs, outputs, 2, N, params);

    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
    }
}

TEST_CASE("ModulationEngine: phaser modulates signal", "[engine][modulation]")
{
    patina::ModulationEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 2048;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::ModulationEngine::Params params;
    params.type = patina::ModulationEngine::Phaser;
    params.rate = 2.0f;
    params.depth = 0.8f;
    params.feedback = 0.5f;
    params.numStages = 4;
    params.mix = 0.5f;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.1f);
}

TEST_CASE("ModulationEngine: tremolo amplitude modulation", "[engine][modulation]")
{
    patina::ModulationEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 2048;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::ModulationEngine::Params params;
    params.type = patina::ModulationEngine::Tremolo;
    params.rate = 5.0f;
    params.depth = 0.8f;
    params.tremoloMode = 0; // Bias
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.01f);
}

TEST_CASE("ModulationEngine: chorus with stereo width", "[engine][modulation]")
{
    patina::ModulationEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 2048;
    float ch0[N], ch1[N];
    for (int i = 0; i < N; ++i) {
        ch0[i] = 0.4f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);
        ch1[i] = ch0[i];
    }

    float out0[N] = {};
    float out1[N] = {};
    const float* ins[] = {ch0, ch1};
    float* outs[] = {out0, out1};

    patina::ModulationEngine::Params params;
    params.type = patina::ModulationEngine::Chorus;
    params.rate = 1.0f;
    params.depth = 0.5f;
    params.chorusDelayMs = 7.0f;
    params.stereoWidth = 0.7f;
    params.mix = 0.5f;
    engine.processBlock(ins, outs, 2, N, params);

    // L and R should differ due to stereo width
    float diff = 0.0f;
    for (int i = 512; i < N; ++i)
        diff += std::fabs(out0[i] - out1[i]);
    REQUIRE(diff > 0.001f);
}

TEST_CASE("ModulationEngine: tremolo modes are selectable", "[engine][modulation]")
{
    patina::ModulationEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    const float* ins[] = {input};

    for (int mode = 0; mode < 3; ++mode)
    {
        engine.reset();
        float output[N] = {};
        float* outs[] = {output};

        patina::ModulationEngine::Params params;
        params.type = patina::ModulationEngine::Tremolo;
        params.tremoloMode = mode;
        params.depth = 0.6f;
        params.rate = 3.0f;
        params.mix = 1.0f;
        engine.processBlock(ins, outs, 1, N, params);

        for (int i = 0; i < N; ++i)
            REQUIRE(std::isfinite(output[i]));
    }
}
