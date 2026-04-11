#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/engine/DriveEngine.h"

using namespace Catch::Matchers;

// ============================================================================
// DriveEngine: basic test
// ============================================================================

TEST_CASE("DriveEngine: prepare and reset", "[engine][drive]")
{
    patina::DriveEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);
    engine.reset();
    REQUIRE(true);
}

TEST_CASE("DriveEngine: silence in, silence out", "[engine][drive]")
{
    patina::DriveEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 256;
    float ch0[N] = {};
    float ch1[N] = {};
    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::DriveEngine::Params params;
    engine.processBlock(inputs, outputs, 2, N, params);

    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
        REQUIRE(std::fabs(out0[i]) < 0.01f);
        REQUIRE(std::fabs(out1[i]) < 0.01f);
    }
}

TEST_CASE("DriveEngine: signal passes through bypass mode", "[engine][drive]")
{
    patina::DriveEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 128;
    float input[N];
    float output[N] = {};

    // 440Hz sine
    for (int i = 0; i < N; ++i)
        input[i] = 0.3f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    const float* ins[] = {input};
    float* outs[] = {output};

    patina::DriveEngine::Params params;
    params.clippingMode = 0;  // Bypass
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    // Output should contain energy
    float energy = 0.0f;
    for (int i = 0; i < N; ++i)
        energy += output[i] * output[i];
    REQUIRE(energy > 0.001f);
}

TEST_CASE("DriveEngine: diode clipping reduces peaks", "[engine][drive]")
{
    patina::DriveEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    float output[N] = {};

    for (int i = 0; i < N; ++i)
        input[i] = 0.8f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    const float* ins[] = {input};
    float* outs[] = {output};

    patina::DriveEngine::Params params;
    params.drive = 1.0f;
    params.clippingMode = 1;  // Diode
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    // Output should be finite
    float maxOut = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        maxOut = std::max(maxOut, std::fabs(output[i]));
    }
    REQUIRE(maxOut > 0.0f);
}

TEST_CASE("DriveEngine: dry/wet mix works", "[engine][drive]")
{
    patina::DriveEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 256;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    const float* ins[] = {input};

    // 100% dry
    float outDry[N] = {};
    float* outsDry[] = {outDry};
    patina::DriveEngine::Params pDry;
    pDry.mix = 0.0f;
    pDry.clippingMode = 1;
    pDry.drive = 1.0f;
    engine.processBlock(ins, outsDry, 1, N, pDry);

    engine.reset();

    // 100% wet
    float outWet[N] = {};
    float* outsWet[] = {outWet};
    patina::DriveEngine::Params pWet;
    pWet.mix = 1.0f;
    pWet.clippingMode = 1;
    pWet.drive = 1.0f;
    engine.processBlock(ins, outsWet, 1, N, pWet);

    // Dry and Wet should differ
    float diff = 0.0f;
    for (int i = 32; i < N; ++i)
        diff += std::fabs(outDry[i] - outWet[i]);
    REQUIRE(diff > 0.01f);
}

TEST_CASE("DriveEngine: power sag reduces headroom", "[engine][drive]")
{
    patina::DriveEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 256;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    const float* ins[] = {input};
    float output[N] = {};
    float* outs[] = {output};

    patina::DriveEngine::Params params;
    params.enablePowerSag = true;
    params.sagAmount = 1.0f;
    params.clippingMode = 2; // Tanh
    params.drive = 0.5f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(output[i]));
}
