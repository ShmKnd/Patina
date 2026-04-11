#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/engine/LimiterEngine.h"

using namespace Catch::Matchers;

// ============================================================================
// LimiterEngine: basic tests
// ============================================================================

TEST_CASE("LimiterEngine: prepare and reset", "[engine][limiter]")
{
    patina::LimiterEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);
    engine.reset();
    REQUIRE(true);
}

TEST_CASE("LimiterEngine: silence in, silence out", "[engine][limiter]")
{
    patina::LimiterEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 256;
    float ch0[N] = {};
    float ch1[N] = {};
    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::LimiterEngine::Params params;
    engine.processBlock(inputs, outputs, 2, N, params);

    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
        REQUIRE(out0[i] == 0.0f);
        REQUIRE(out1[i] == 0.0f);
    }
}

// ============================================================================
// Type-specific signal processing tests
// ============================================================================

TEST_CASE("LimiterEngine: VCA limiter processes signal", "[engine][limiter]")
{
    patina::LimiterEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.9f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::LimiterEngine::Params params;
    params.type = patina::LimiterEngine::Vca;
    params.ceiling = 0.5f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.001f);
}

TEST_CASE("LimiterEngine: FET limiter processes signal", "[engine][limiter]")
{
    patina::LimiterEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.8f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::LimiterEngine::Params params;
    params.type = patina::LimiterEngine::Fet;
    params.ceiling = 0.6f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.001f);
}

TEST_CASE("LimiterEngine: Opto limiter processes signal", "[engine][limiter]")
{
    patina::LimiterEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.7f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::LimiterEngine::Params params;
    params.type = patina::LimiterEngine::Opto;
    params.ceiling = 0.5f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.001f);
}

// ============================================================================
// Ceiling clamp test
// ============================================================================

TEST_CASE("LimiterEngine: output ceiling clamp works", "[engine][limiter]")
{
    patina::LimiterEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 2048;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.95f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    // ceiling=0.5 -> -10dBFS -> ~0.316 linear
    patina::LimiterEngine::Params params;
    params.type = patina::LimiterEngine::Vca;
    params.ceiling = 0.5f;
    params.outputGain = 0.5f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs, 1, N, params);

    const double ceilingLin = std::pow(10.0, (-20.0 + 0.5 * 20.0) / 20.0);
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        REQUIRE(std::abs(output[i]) <= (float)(ceilingLin + 0.01));
    }
}

// ============================================================================
// Gain reduction reporting test
// ============================================================================

TEST_CASE("LimiterEngine: gain reduction reporting", "[engine][limiter]")
{
    patina::LimiterEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 1024;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.9f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    // VCA limiter
    patina::LimiterEngine::Params params;
    params.type = patina::LimiterEngine::Vca;
    params.ceiling = 0.3f;
    engine.processBlock(ins, outs, 1, N, params);

    REQUIRE(std::isfinite(engine.getVcaGainReductionDb(0)));
    REQUIRE(std::isfinite(engine.getGainReductionDb(0)));

    // FET limiter
    params.type = patina::LimiterEngine::Fet;
    engine.processBlock(ins, outs, 1, N, params);
    REQUIRE(std::isfinite(engine.getFetGainReductionDb(0)));

    // Opto limiter
    params.type = patina::LimiterEngine::Opto;
    engine.processBlock(ins, outs, 1, N, params);
    REQUIRE(std::isfinite(engine.getOptoGainReductionDb(0)));
}

// ============================================================================
// pedalMode test
// ============================================================================

TEST_CASE("LimiterEngine: pedal mode signal path", "[engine][limiter]")
{
    patina::LimiterEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float outBoard[N] = {};
    float outPedal[N] = {};
    const float* ins[] = {input};
    float* outsB[] = {outBoard};
    float* outsP[] = {outPedal};

    // Outboard (default)
    patina::LimiterEngine::Params params;
    params.type = patina::LimiterEngine::Vca;
    params.ceiling = 0.7f;
    params.mix = 1.0f;
    engine.processBlock(ins, outsB, 1, N, params);

    // Pedal
    engine.reset();
    params.pedalMode = true;
    engine.processBlock(ins, outsP, 1, N, params);

    // Both should be finite and produce signal
    float energyB = 0.0f, energyP = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(outBoard[i]));
        REQUIRE(std::isfinite(outPedal[i]));
        energyB += outBoard[i] * outBoard[i];
        energyP += outPedal[i] * outPedal[i];
    }
    REQUIRE(energyB > 0.001f);
    REQUIRE(energyP > 0.001f);
}

// ============================================================================
// Dry/Wet mix test
// ============================================================================

TEST_CASE("LimiterEngine: dry/wet mix", "[engine][limiter]")
{
    patina::LimiterEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.8f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output0[N] = {};
    float output1[N] = {};
    const float* ins[] = {input};
    float* outs0[] = {output0};
    float* outs1[] = {output1};

    // Full wet
    patina::LimiterEngine::Params params;
    params.type = patina::LimiterEngine::Vca;
    params.ceiling = 0.5f;
    params.mix = 1.0f;
    engine.processBlock(ins, outs0, 1, N, params);

    // 50% wet
    engine.reset();
    params.mix = 0.5f;
    engine.processBlock(ins, outs1, 1, N, params);

    // Outputs should differ (mix is applied)
    bool differs = false;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output0[i]));
        REQUIRE(std::isfinite(output1[i]));
        if (std::abs(output0[i] - output1[i]) > 1e-6f) {
            differs = true;
            break;
        }
    }
    REQUIRE(differs);
}
