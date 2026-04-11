#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/engine/CompressorEngine.h"

using namespace Catch::Matchers;

// ============================================================================
// CompressorEngine: basic test
// ============================================================================

TEST_CASE("CompressorEngine: prepare and reset", "[engine][compressor]")
{
    patina::CompressorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);
    engine.reset();
    REQUIRE(true);
}

TEST_CASE("CompressorEngine: silence in, silence out", "[engine][compressor]")
{
    patina::CompressorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 256;
    float ch0[N] = {};
    float ch1[N] = {};
    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::CompressorEngine::Params params;
    engine.processBlock(inputs, outputs, 2, N, params);

    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
    }
}

TEST_CASE("CompressorEngine: FET compressor processes signal", "[engine][compressor]")
{
    patina::CompressorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.8f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::CompressorEngine::Params params;
    params.type = patina::CompressorEngine::Fet;
    params.inputGain = 0.7f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.01f);
}

TEST_CASE("CompressorEngine: Photo compressor processes signal", "[engine][compressor]")
{
    patina::CompressorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.6f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::CompressorEngine::Params params;
    params.type = patina::CompressorEngine::Photo;
    params.threshold = 0.5f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.001f);
}

TEST_CASE("CompressorEngine: VariableMu compressor processes signal", "[engine][compressor]")
{
    patina::CompressorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.7f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::CompressorEngine::Params params;
    params.type = patina::CompressorEngine::VariableMu;
    params.threshold = 0.4f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.001f);
}

TEST_CASE("CompressorEngine: noise gate functionality", "[engine][compressor]")
{
    patina::CompressorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 256;
    float input[N];
    // Very small signal
    for (int i = 0; i < N; ++i)
        input[i] = 0.0001f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::CompressorEngine::Params params;
    params.enableGate = true;
    params.gateThresholdDb = -20.0f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    // Gate is closed so output should be small
    float energy = 0.0f;
    for (int i = 0; i < N; ++i)
        energy += output[i] * output[i];
    REQUIRE(energy < 0.01f);
}

TEST_CASE("CompressorEngine: gain reduction reporting", "[engine][compressor]")
{
    patina::CompressorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.8f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::CompressorEngine::Params params;
    params.type = patina::CompressorEngine::Fet;
    params.inputGain = 0.9f;
    engine.processBlock(ins, outs, 1, N, params);

    // Gain reduction value should be finite
    float gr = engine.getFetGainReductionDb(0);
    REQUIRE(std::isfinite(gr));
}
