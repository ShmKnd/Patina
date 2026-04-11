#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "dsp/circuits/modulation/EnvelopeGenerator.h"
#include "dsp/engine/EnvelopeGeneratorEngine.h"

using namespace Catch::Matchers;

static constexpr double kSampleRate = 44100.0;

// ============================================================================
// L3: EnvelopeGenerator circuit tests
// ============================================================================

TEST_CASE("EnvelopeGenerator: prepare and reset", "[circuit][envgen]")
{
    EnvelopeGenerator eg;
    eg.prepare(2, kSampleRate);
    eg.reset();
    REQUIRE(true);
}

TEST_CASE("EnvelopeGenerator: prepare with ProcessSpec", "[circuit][envgen]")
{
    EnvelopeGenerator eg;
    patina::ProcessSpec spec{kSampleRate, 512, 2};
    eg.prepare(spec);
    eg.reset();
    REQUIRE(true);
}

TEST_CASE("EnvelopeGenerator: idle state outputs zero", "[circuit][envgen]")
{
    EnvelopeGenerator eg;
    eg.prepare(1, kSampleRate);

    EnvelopeGenerator::Params params;
    for (int i = 0; i < 100; ++i)
    {
        float env = eg.process(0, params);
        REQUIRE(env >= 0.0f);
        REQUIRE(env <= 0.001f);
    }
    REQUIRE(eg.getStage(0) == EnvelopeGenerator::Stage::Idle);
}

TEST_CASE("EnvelopeGenerator: ADSR cycle", "[circuit][envgen]")
{
    EnvelopeGenerator eg;
    eg.prepare(1, kSampleRate);

    EnvelopeGenerator::Params params;
    params.attack  = 0.1f;   // short attack (~5ms)
    params.decay   = 0.1f;   // short decay (~50ms)
    params.sustain = 0.5f;
    params.release = 0.1f;   // short release (~100ms)
    params.mode    = 0;       // ADSR
    params.curve   = 0;       // RC

    // === Gate ON -> Attack ===
    eg.gateOn(0);
    REQUIRE(eg.getStage(0) == EnvelopeGenerator::Stage::Attack);

    // Attack: envelope rises
    float prevEnv = 0.0f;
    bool reachedPeak = false;
    for (int i = 0; i < 22050; ++i) // up to 500ms (accounts for slow convergence when OTA saturates)
    {
        float env = eg.process(0, params);
        if (eg.getStage(0) != EnvelopeGenerator::Stage::Attack)
        {
            reachedPeak = true;
            break;
        }
        if (i > 0) REQUIRE(env >= prevEnv - 0.001f); // monotonic increase
        prevEnv = env;
    }
    REQUIRE(reachedPeak);

    // Decay: converges to sustain level
    for (int i = 0; i < 44100; ++i) // up to 1s
    {
        float env = eg.process(0, params);
        if (eg.getStage(0) == EnvelopeGenerator::Stage::Sustain)
            break;
    }
    REQUIRE(eg.getStage(0) == EnvelopeGenerator::Stage::Sustain);

    // Envelope during sustain ~ sustain level
    float sustainEnv = eg.process(0, params);
    REQUIRE(sustainEnv > 0.4f);
    REQUIRE(sustainEnv < 0.6f);

    // === Gate OFF -> Release ===
    eg.gateOff(0);
    REQUIRE(eg.getStage(0) == EnvelopeGenerator::Stage::Release);

    // Release: envelope decreases -> Idle
    for (int i = 0; i < 44100; ++i)
    {
        float env = eg.process(0, params);
        if (eg.getStage(0) == EnvelopeGenerator::Stage::Idle)
            break;
    }
    REQUIRE(eg.getStage(0) == EnvelopeGenerator::Stage::Idle);
    REQUIRE(eg.getEnvelope(0) < 0.01f);
}

TEST_CASE("EnvelopeGenerator: AD mode returns to idle", "[circuit][envgen]")
{
    EnvelopeGenerator eg;
    eg.prepare(1, kSampleRate);

    EnvelopeGenerator::Params params;
    params.attack  = 0.05f;
    params.decay   = 0.1f;
    params.mode    = 1; // AD

    eg.gateOn(0);

    // AD mode: even if gate held, returns to idle after decay
    for (int i = 0; i < 44100; ++i)
    {
        eg.process(0, params);
        if (eg.getStage(0) == EnvelopeGenerator::Stage::Idle)
            break;
    }
    REQUIRE(eg.getStage(0) == EnvelopeGenerator::Stage::Idle);
}

TEST_CASE("EnvelopeGenerator: retrigger during release", "[circuit][envgen]")
{
    EnvelopeGenerator eg;
    eg.prepare(1, kSampleRate);

    EnvelopeGenerator::Params params;
    params.attack  = 0.1f;
    params.decay   = 0.1f;
    params.sustain = 0.5f;
    params.release = 0.2f;

    // Gate ON -> until sustain
    eg.gateOn(0);
    for (int i = 0; i < 44100; ++i)
    {
        eg.process(0, params);
        if (eg.getStage(0) == EnvelopeGenerator::Stage::Sustain)
            break;
    }

    // Gate OFF -> start release
    eg.gateOff(0);
    for (int i = 0; i < 1000; ++i)
        eg.process(0, params);

    float releaseEnv = eg.getEnvelope(0);
    REQUIRE(releaseEnv > 0.0f);

    // Retrigger: gate ON during release -> returns to attack
    eg.gateOn(0);
    eg.process(0, params);
    REQUIRE(eg.getStage(0) == EnvelopeGenerator::Stage::Attack);
}

TEST_CASE("EnvelopeGenerator: linear curve", "[circuit][envgen]")
{
    EnvelopeGenerator eg;
    eg.prepare(1, kSampleRate);

    EnvelopeGenerator::Params params;
    params.attack  = 0.1f;
    params.sustain = 0.0f;
    params.decay   = 0.1f;
    params.mode    = 1; // AD
    params.curve   = 1; // Linear

    eg.gateOn(0);

    float prevEnv = 0.0f;
    bool rising = true;
    for (int i = 0; i < 44100; ++i)
    {
        float env = eg.process(0, params);
        if (rising && eg.getStage(0) != EnvelopeGenerator::Stage::Attack)
        {
            rising = false;
            REQUIRE(env >= 0.99f); // Peak reached
        }
        if (eg.getStage(0) == EnvelopeGenerator::Stage::Idle)
            break;
        prevEnv = env;
    }
    REQUIRE(eg.getStage(0) == EnvelopeGenerator::Stage::Idle);
}

TEST_CASE("EnvelopeGenerator: envelope stays in [0, 1]", "[circuit][envgen]")
{
    EnvelopeGenerator eg;
    eg.prepare(1, kSampleRate);

    EnvelopeGenerator::Params params;
    params.attack  = 0.01f;  // Very short attack
    params.decay   = 0.5f;
    params.sustain = 0.8f;
    params.release = 0.5f;

    // Multiple gate ON/OFF cycles
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        eg.gateOn(0);
        for (int i = 0; i < 5000; ++i)
        {
            float env = eg.process(0, params);
            REQUIRE(env >= 0.0f);
            REQUIRE(env <= 1.0f);
        }
        eg.gateOff(0);
        for (int i = 0; i < 5000; ++i)
        {
            float env = eg.process(0, params);
            REQUIRE(env >= 0.0f);
            REQUIRE(env <= 1.0f);
        }
    }
}

// ============================================================================
// L3: EnvelopeGenerator - L2 parts integration test
// ============================================================================

TEST_CASE("EnvelopeGenerator: L2 parts are initialized", "[circuit][envgen]")
{
    EnvelopeGenerator eg;
    eg.prepare(1, kSampleRate);

    // DiodePrimitive: Si1N4148  Vf 
    double vf = eg.getSwitchDiode().effectiveVf(25.0);
    REQUIRE(vf > 0.5);
    REQUIRE(vf < 0.8);

    // OTA_Primitive: CA3080  gm  25degC  ~1.0
    double gm = eg.getCurrentSource().gmScale(25.0);
    REQUIRE(gm > 0.95);
    REQUIRE(gm < 1.05);

    // BJT_Primitive: Matched 
    double mismatch = eg.getCurrentMirror().getMismatch();
    REQUIRE(mismatch > 0.98);
    REQUIRE(mismatch < 1.02);
}

TEST_CASE("EnvelopeGenerator: temperature affects timing via L2 parts", "[circuit][envgen]")
{
    // 25degC  50degC 
    // OTA gmScale + BJT tempScale 
    EnvelopeGenerator eg25, eg50;
    eg25.prepare(1, kSampleRate);
    eg50.prepare(1, kSampleRate);

    EnvelopeGenerator::Params p25, p50;
    p25.attack = 0.3f;
    p25.temperature = 25.0f;
    p50.attack = 0.3f;
    p50.temperature = 50.0f;

    eg25.gateOn(0);
    eg50.gateOn(0);

    // 500 
    float env25 = 0.0f, env50 = 0.0f;
    for (int i = 0; i < 500; ++i)
    {
        env25 = eg25.process(0, p25);
        env50 = eg50.process(0, p50);
    }

    // 
    REQUIRE(env25 != env50);
}

TEST_CASE("EnvelopeGenerator: OTA saturation compresses attack curve", "[circuit][envgen]")
{
    // RC OTA
    EnvelopeGenerator eg;
    eg.prepare(1, kSampleRate);

    EnvelopeGenerator::Params params;
    params.attack = 0.2f;
    params.curve = 0; // RC

    eg.gateOn(0);

    //  (50)
    float earlyEnv = 0.0f;
    for (int i = 0; i < 50; ++i)
        earlyEnv = eg.process(0, params);
    float earlyRate = earlyEnv / 50.0f;

    //  (env > 0.7 50)
    float prevEnv = earlyEnv;
    float lateRate = 0.0f;
    for (int i = 0; i < 10000; ++i)
    {
        float env = eg.process(0, params);
        if (prevEnv > 0.7f && prevEnv < 0.9f)
        {
            lateRate = (env - prevEnv);
            break;
        }
        prevEnv = env;
    }

    // RC + OTA: 
    REQUIRE(earlyRate > lateRate);
}

TEST_CASE("EnvelopeGenerator: diode affects release tail", "[circuit][envgen]")
{
    // 
    EnvelopeGenerator eg;
    eg.prepare(1, kSampleRate);

    EnvelopeGenerator::Params params;
    params.attack = 0.05f;
    params.sustain = 0.8f;
    params.release = 0.2f;
    params.curve = 0; // RC

    // 
    eg.gateOn(0);
    for (int i = 0; i < 44100; ++i)
    {
        eg.process(0, params);
        if (eg.getStage(0) == EnvelopeGenerator::Stage::Sustain) break;
    }

    // 
    eg.gateOff(0);

    // 
    float prevEnv = eg.getEnvelope(0);
    float earlyDecayTotal = 0.0f;
    float lateDecayTotal = 0.0f;
    int earlyCount = 0, lateCount = 0;

    for (int i = 0; i < 44100; ++i)
    {
        float env = eg.process(0, params);
        if (eg.getStage(0) == EnvelopeGenerator::Stage::Idle) break;

        float decay = prevEnv - env;
        if (prevEnv > 0.3f) { earlyDecayTotal += decay; earlyCount++; }
        else if (prevEnv > 0.01f) { lateDecayTotal += decay; lateCount++; }
        prevEnv = env;
    }

    // 1
    if (earlyCount > 0 && lateCount > 0)
    {
        float earlyAvg = earlyDecayTotal / (float)earlyCount;
        float lateAvg  = lateDecayTotal / (float)lateCount;
        REQUIRE(earlyAvg > lateAvg);
    }
}

// ============================================================================
// L4: EnvelopeGeneratorEngine 
// ============================================================================

TEST_CASE("EnvelopeGeneratorEngine: prepare and reset", "[engine][envgen]")
{
    patina::EnvelopeGeneratorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);
    engine.reset();
    REQUIRE(true);
}

TEST_CASE("EnvelopeGeneratorEngine: silence in, silence out", "[engine][envgen]")
{
    patina::EnvelopeGeneratorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 256;
    float ch0[N] = {};
    float ch1[N] = {};
    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::EnvelopeGeneratorEngine::Params params;
    engine.processBlock(inputs, outputs, 2, N, params);

    for (int i = 0; i < N; ++i)
    {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
    }
}

TEST_CASE("EnvelopeGeneratorEngine: external gate VCA modulation", "[engine][envgen]")
{
    patina::EnvelopeGeneratorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    // 1kHz 
    constexpr int N = 4096;
    float ch0[N], ch1[N];
    for (int i = 0; i < N; ++i)
    {
        float v = 0.5f * std::sin(2.0f * 3.14159265f * 1000.0f * (float)i / 44100.0f);
        ch0[i] = v;
        ch1[i] = v;
    }
    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::EnvelopeGeneratorEngine::Params params;
    params.attack    = 0.05f;
    params.sustain   = 0.8f;
    params.vcaDepth  = 1.0f;
    params.mix       = 1.0f;

    // OFF: VCA  -> 
    engine.processBlock(inputs, outputs, 2, N, params);

    float maxNoGate = 0.0f;
    for (int i = 0; i < N; ++i)
        maxNoGate = std::max(maxNoGate, std::abs(out0[i]));

    // ON: VCA  -> 
    engine.gateOn();
    engine.processBlock(inputs, outputs, 2, N, params);

    float maxWithGate = 0.0f;
    for (int i = 128; i < N; ++i)  // 
        maxWithGate = std::max(maxWithGate, std::abs(out0[i]));

    REQUIRE(maxWithGate > maxNoGate + 0.01f);
}

TEST_CASE("EnvelopeGeneratorEngine: auto trigger mode", "[engine][envgen]")
{
    patina::EnvelopeGeneratorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 2048;
    float ch0[N];
    for (int i = 0; i < N; ++i)
        ch0[i] = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * (float)i / 44100.0f);
    float ch1[N];
    std::copy(ch0, ch0 + N, ch1);

    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::EnvelopeGeneratorEngine::Params params;
    params.triggerMode     = patina::EnvelopeGeneratorEngine::Auto;
    params.autoThresholdDb = -20.0f;
    params.attack          = 0.05f;
    params.vcaDepth        = 1.0f;

    engine.processBlock(inputs, outputs, 2, N, params);

    // : 
    float env = engine.getEnvelope(0);
    REQUIRE(env > 0.0f);
}

TEST_CASE("EnvelopeGeneratorEngine: pedal mode", "[engine][envgen]")
{
    patina::EnvelopeGeneratorEngine engine;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine.prepare(spec);

    constexpr int N = 512;
    float ch0[N];
    for (int i = 0; i < N; ++i)
        ch0[i] = 0.3f * std::sin(2.0f * 3.14159265f * 440.0f * (float)i / 44100.0f);
    float ch1[N];
    std::copy(ch0, ch0 + N, ch1);

    const float* inputs[] = {ch0, ch1};
    float out0[N] = {};
    float out1[N] = {};
    float* outputs[] = {out0, out1};

    patina::EnvelopeGeneratorEngine::Params params;
    params.pedalMode     = true;
    params.supplyVoltage = 9.0;
    params.triggerMode   = patina::EnvelopeGeneratorEngine::Auto;
    params.autoThresholdDb = -20.0f;

    engine.gateOn();
    engine.processBlock(inputs, outputs, 2, N, params);

    for (int i = 0; i < N; ++i)
    {
        REQUIRE(std::isfinite(out0[i]));
        REQUIRE(std::isfinite(out1[i]));
    }
}

TEST_CASE("EnvelopeGeneratorEngine: velocity scales output", "[engine][envgen]")
{
    patina::EnvelopeGeneratorEngine engineHi, engineLo;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engineHi.prepare(spec);
    engineLo.prepare(spec);

    constexpr int N = 2048;
    float ch0[N];
    for (int i = 0; i < N; ++i)
        ch0[i] = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * (float)i / 44100.0f);
    float ch1[N];
    std::copy(ch0, ch0 + N, ch1);

    const float* inputs[] = {ch0, ch1};
    float outHi0[N] = {}, outHi1[N] = {};
    float outLo0[N] = {}, outLo1[N] = {};
    float* outputsHi[] = {outHi0, outHi1};
    float* outputsLo[] = {outLo0, outLo1};

    patina::EnvelopeGeneratorEngine::Params paramsHi, paramsLo;
    paramsHi.attack   = 0.05f;
    paramsHi.sustain  = 0.8f;
    paramsHi.vcaDepth = 1.0f;
    paramsHi.velocity = 1.0f;    // 
    paramsHi.mix      = 1.0f;

    paramsLo = paramsHi;
    paramsLo.velocity = 0.3f;    // 

    engineHi.gateOn();
    engineLo.gateOn();
    engineHi.processBlock(inputs, outputsHi, 2, N, paramsHi);
    engineLo.processBlock(inputs, outputsLo, 2, N, paramsLo);

    // : 
    float maxHi = 0.0f, maxLo = 0.0f;
    for (int i = N / 2; i < N; ++i)
    {
        maxHi = std::max(maxHi, std::abs(outHi0[i]));
        maxLo = std::max(maxLo, std::abs(outLo0[i]));
    }
    REQUIRE(maxHi > maxLo);
}

TEST_CASE("EnvelopeGeneratorEngine: temperature affects VCA", "[engine][envgen]")
{
    patina::EnvelopeGeneratorEngine engine25, engine50;
    patina::ProcessSpec spec{44100.0, 512, 2};
    engine25.prepare(spec);
    engine50.prepare(spec);

    constexpr int N = 1024;
    float ch0[N];
    for (int i = 0; i < N; ++i)
        ch0[i] = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * (float)i / 44100.0f);
    float ch1[N];
    std::copy(ch0, ch0 + N, ch1);

    const float* inputs[] = {ch0, ch1};
    float out25_0[N] = {}, out25_1[N] = {};
    float out50_0[N] = {}, out50_1[N] = {};
    float* outputs25[] = {out25_0, out25_1};
    float* outputs50[] = {out50_0, out50_1};

    patina::EnvelopeGeneratorEngine::Params p25, p50;
    p25.attack = 0.05f;
    p25.sustain = 0.8f;
    p25.vcaDepth = 1.0f;
    p25.temperature = 25.0f;
    p25.mix = 1.0f;

    p50 = p25;
    p50.temperature = 50.0f;

    engine25.gateOn();
    engine50.gateOn();
    engine25.processBlock(inputs, outputs25, 2, N, p25);
    engine50.processBlock(inputs, outputs50, 2, N, p50);

    // 
    bool allSame = true;
    for (int i = 100; i < N; ++i)
    {
        if (std::abs(out25_0[i] - out50_0[i]) > 1e-6f)
        {
            allSame = false;
            break;
        }
    }
    REQUIRE_FALSE(allSame);
}
