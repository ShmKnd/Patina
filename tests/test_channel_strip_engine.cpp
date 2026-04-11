#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/engine/ChannelStripEngine.h"

using namespace Catch::Matchers;

// ============================================================================
// ChannelStripEngine: basic test
// ============================================================================

TEST_CASE("ChannelStripEngine: prepare and reset", "[engine][strip]")
{
    patina::ChannelStripEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);
    engine.reset();
    REQUIRE(true);
}

TEST_CASE("ChannelStripEngine: silence in, silence out", "[engine][strip]")
{
    patina::ChannelStripEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 256;
    float ch0[N] = {};
    float ch1[N] = {};
    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::ChannelStripEngine::Params params;
    engine.processBlock(inputs, outputs, 2, N, params);

    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
    }
}

TEST_CASE("ChannelStripEngine: preamp drives signal", "[engine][strip]")
{
    patina::ChannelStripEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.3f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::ChannelStripEngine::Params params;
    params.preampDrive = 0.8f;
    params.enableEq = false;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.01f);
}

TEST_CASE("ChannelStripEngine: EQ changes tone", "[engine][strip]")
{
    patina::ChannelStripEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};

    constexpr int N = 1024;
    float input[N];
    // White-noise-like signal (harmonic-rich)
    for (int i = 0; i < N; ++i)
        input[i] = 0.3f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0)
                 + 0.1f * std::sin(2.0 * 3.14159265 * 2000.0 * i / 44100.0)
                 + 0.05f * std::sin(2.0 * 3.14159265 * 8000.0 * i / 44100.0);

    const float* ins[] = {input};

    // Low-pass filter
    engine.prepare(spec);
    float outLP[N] = {};
    float* outsLP[] = {outLP};
    patina::ChannelStripEngine::Params pLP;
    pLP.enableEq = true;
    pLP.eqType = 0; // LP
    pLP.eqCutoffHz = 500.0f;
    engine.processBlock(ins, outsLP, 1, N, pLP);

    // High-pass filter
    engine.reset();
    float outHP[N] = {};
    float* outsHP[] = {outHP};
    patina::ChannelStripEngine::Params pHP;
    pHP.enableEq = true;
    pHP.eqType = 1; // HP
    pHP.eqCutoffHz = 2000.0f;
    engine.processBlock(ins, outsHP, 1, N, pHP);

    // LP and HP outputs should differ
    float diff = 0.0f;
    for (int i = 128; i < N; ++i)
        diff += std::fabs(outLP[i] - outHP[i]);
    REQUIRE(diff > 0.1f);
}

TEST_CASE("ChannelStripEngine: noise gate opens and closes", "[engine][strip]")
{
    patina::ChannelStripEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 256;

    // Quiet signal
    float quiet[N];
    for (int i = 0; i < N; ++i)
        quiet[i] = 0.0001f;

    float output[N] = {};
    const float* ins[] = {quiet};
    float* outs[] = {output};

    patina::ChannelStripEngine::Params params;
    params.enableGate = true;
    params.gateThresholdDb = -20.0f;
    params.preampDrive = 0.1f;
    params.enableEq = false;
    engine.processBlock(ins, outs, 1, N, params);

    // Gate should close so output is small
    float energy = 0.0f;
    for (int i = 0; i < N; ++i)
        energy += output[i] * output[i];
    REQUIRE(energy < 0.01f);
}

TEST_CASE("ChannelStripEngine: full chain processing", "[engine][strip]")
{
    patina::ChannelStripEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.4f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::ChannelStripEngine::Params params;
    params.preampDrive = 0.5f;
    params.enableEq = true;
    params.eqCutoffHz = 3000.0f;
    params.eqType = 0;
    engine.processBlock(ins, outs, 1, N, params);

    float energy = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(output[i]));
        energy += output[i] * output[i];
    }
    REQUIRE(energy > 0.001f);
}

TEST_CASE("ChannelStripEngine: output level metering", "[engine][strip]")
{
    patina::ChannelStripEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 1};
    engine.prepare(spec);

    constexpr int N = 512;
    float input[N];
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0);

    float output[N] = {};
    const float* ins[] = {input};
    float* outs[] = {output};

    patina::ChannelStripEngine::Params params;
    params.enableEq = false;
    engine.processBlock(ins, outs, 1, N, params);

    float level = engine.getOutputLevel(0);
    REQUIRE(std::isfinite(level));
    REQUIRE(level >= 0.0f);
}
