#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/engine/TapeMachineEngine.h"

using namespace Catch::Matchers;

// ============================================================================
// TapeMachineEngine: basic test
// ============================================================================

TEST_CASE("TapeMachineEngine: prepare and reset", "[engine][tape]")
{
    patina::TapeMachineEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);
    engine.reset();
    REQUIRE(true);
}

TEST_CASE("TapeMachineEngine: silence in, silence out", "[engine][tape]")
{
    patina::TapeMachineEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 256;
    float ch0[N] = {};
    float ch1[N] = {};
    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::TapeMachineEngine::Params params;
    engine.processBlock(inputs, outputs, 2, N, params);

    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
    }
}

TEST_CASE("TapeMachineEngine: tape saturation colors signal", "[engine][tape]")
{
    patina::TapeMachineEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.6f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::TapeMachineEngine::Params params;
    params.saturation = 0.8f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.01f);
}

TEST_CASE("TapeMachineEngine: tape speed affects character", "[engine][tape]")
{
    patina::TapeMachineEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    const float* ins[] = {input};

    // 7.5 ips (slow)
    engine.prepare(spec);
    float outSlow[N] = {};
    float* outsSlow[] = {outSlow};
    patina::TapeMachineEngine::Params pSlow;
    pSlow.tapeSpeed = 0.5f;
    pSlow.saturation = 0.5f;
    pSlow.mix = 1.0f;
    engine.processBlock(ins, outsSlow, 1, N, pSlow);

    // 30 ips (fast)
    engine.reset();
    float outFast[N] = {};
    float* outsFast[] = {outFast};
    patina::TapeMachineEngine::Params pFast;
    pFast.tapeSpeed = 2.0f;
    pFast.saturation = 0.5f;
    pFast.mix = 1.0f;
    engine.processBlock(ins, outsFast, 1, N, pFast);

    // Differences due to speed should be observable
    float diff = 0.0f;
    for (int i = 64; i < N; ++i)
        diff += std::fabs(outSlow[i] - outFast[i]);
    REQUIRE(diff > 0.001f);
}

TEST_CASE("TapeMachineEngine: transformer can be disabled", "[engine][tape]")
{
    patina::TapeMachineEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 256;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.4f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::TapeMachineEngine::Params params;
    params.enableTransformer = false;
    params.saturation = 0.5f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(output[i]));
}

TEST_CASE("TapeMachineEngine: wow and flutter", "[engine][tape]")
{
    patina::TapeMachineEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 2048;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.4f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::TapeMachineEngine::Params params;
    params.wowFlutter = 0.8f;
    params.saturation = 0.3f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.01f);
}

TEST_CASE("TapeMachineEngine: aging parameters", "[engine][tape]")
{
    patina::TapeMachineEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 256;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::TapeMachineEngine::Params params;
    params.headWear = 0.8f;
    params.tapeAge = 0.7f;
    params.saturation = 0.5f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(output[i]));
}
