#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/engine/ReverbEngine.h"

using namespace Catch::Matchers;

// ============================================================================
// ReverbEngine: basic test
// ============================================================================

TEST_CASE("ReverbEngine: prepare and reset", "[engine][reverb]")
{
    patina::ReverbEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);
    engine.reset();
    REQUIRE(true);
}

TEST_CASE("ReverbEngine: silence in, silence out", "[engine][reverb]")
{
    patina::ReverbEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 256;
    float ch0[N] = {};
    float ch1[N] = {};
    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::ReverbEngine::Params params;
    engine.processBlock(inputs, outputs, 2, N, params);

    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
        REQUIRE(std::fabs(out0[i]) < 0.01f);
        REQUIRE(std::fabs(out1[i]) < 0.01f);
    }
}

TEST_CASE("ReverbEngine: spring reverb produces tail", "[engine][reverb]")
{
    patina::ReverbEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    // Impulse input
    constexpr int N = 4096;
    float input[N] = {};
    input[0] = 0.5f;
    float output[N] = {};

    const float* ins[] = {input};
    float* outs[] = {output};

    patina::ReverbEngine::Params params;
    params.type = patina::ReverbEngine::Spring;
    params.decay = 0.7f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    // Reverb tail: there should be energy in the later part
    float lateEnergy = 0.0f;
    for (int i = 2000; i < N; ++i)
        lateEnergy += output[i] * output[i];
    REQUIRE(lateEnergy > 1e-8f);
}

TEST_CASE("ReverbEngine: plate reverb produces tail", "[engine][reverb]")
{
    patina::ReverbEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 4096;
    float input[N] = {};
    input[0] = 0.5f;
    float output[N] = {};

    const float* ins[] = {input};
    float* outs[] = {output};

    patina::ReverbEngine::Params params;
    params.type = patina::ReverbEngine::Plate;
    params.decay = 0.7f;
    params.mix = 1.0f;
    params.predelayMs = 5.0f;
    engine.processBlock(ins, outs, 1, N, params);

    float lateEnergy = 0.0f;
    for (int i = 2000; i < N; ++i)
        lateEnergy += output[i] * output[i];
    REQUIRE(lateEnergy > 1e-8f);
}

TEST_CASE("ReverbEngine: dry/wet mix at 0 passes original", "[engine][reverb]")
{
    patina::ReverbEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 256;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.4f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::ReverbEngine::Params params;
    params.mix = 0.0f;
    engine.processBlock(ins, outs, 1, N, params);

    // Mix=0 should be close to dry signal
    for (int i = 32; i < N; ++i)
        REQUIRE(std::fabs(output[i] - input[i]) < 0.05f);
}
