#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/core/ProcessSpec.h"
#include "dsp/circuits/drive/DiodeClipper.h"
#include "dsp/circuits/drive/OutputStage.h"
#include "dsp/circuits/drive/InputFilter.h"
#include "dsp/circuits/drive/InputBuffer.h"
#include "dsp/circuits/modulation/AnalogLfo.h"
#include "dsp/circuits/compander/CompanderModule.h"
#include "dsp/circuits/bbd/BbdStageEmulator.h"
#include "dsp/circuits/bbd/BbdSampler.h"
#include "dsp/circuits/bbd/BbdFeedback.h"
#include "dsp/circuits/filters/ToneFilter.h"

using namespace Catch::Matchers;

// ============================================================================
// ProcessSpec: prepare() integration test
// ============================================================================

TEST_CASE("ProcessSpec: DiodeClipper prepare overload", "[processspec]")
{
    patina::ProcessSpec spec{44100.0, 512, 2};
    DiodeClipper dm;
    dm.prepare(spec);
    // Confirm it processes normally
    float out = dm.process(0, 0.5f, 0.5f, 2);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("ProcessSpec: OutputStage prepare overload", "[processspec]")
{
    patina::ProcessSpec spec{48000.0, 256, 2};
    OutputStage om;
    om.prepare(spec);
    om.setCutoffHz(4000.0);
    float out = om.process(0, 0.5f, 9.0);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("ProcessSpec: InputFilter prepare overload", "[processspec]")
{
    patina::ProcessSpec spec{44100.0, 512, 2};
    InputFilter lpf;
    lpf.prepare(spec);
    float out = lpf.process(0, 0.5f);
    REQUIRE(std::isfinite(out));
    REQUIRE(lpf.getNumChannels() == 2);
}

TEST_CASE("ProcessSpec: InputBuffer prepare overload", "[processspec]")
{
    patina::ProcessSpec spec{44100.0, 512, 2};
    InputBuffer buf;
    buf.prepare(spec);
    float out = buf.process(0, 0.3f);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("ProcessSpec: AnalogLfo prepare overload", "[processspec]")
{
    patina::ProcessSpec spec{44100.0, 512, 2};
    AnalogLfo lfo;
    lfo.prepare(spec);
    lfo.stepAll();
    REQUIRE(std::isfinite(lfo.getTri(0)));
    REQUIRE(std::isfinite(lfo.getSinLike(1)));
}

TEST_CASE("ProcessSpec: CompanderModule prepare overload", "[processspec]")
{
    patina::ProcessSpec spec{44100.0, 512, 2};
    CompanderModule cm;
    cm.prepare(spec);
    float out = cm.processCompress(0, 0.3f, 0.5f);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("ProcessSpec: BbdStageEmulator prepare overload", "[processspec]")
{
    patina::ProcessSpec spec{44100.0, 512, 2};
    BbdStageEmulator bbd;
    bbd.prepare(spec);
    std::vector<float> frame = {0.5f, -0.3f};
    bbd.process(frame, 4.0, 4096, 9.0, false, 0.0);
    REQUIRE(std::isfinite(frame[0]));
    REQUIRE(std::isfinite(frame[1]));
}

TEST_CASE("ProcessSpec: BbdSampler prepare overload", "[processspec]")
{
    patina::ProcessSpec spec{44100.0, 512, 2};
    BbdSampler sampler;
    sampler.prepare(spec);
    std::minstd_rand rng(42);
    std::normal_distribution<double> nd(0.0, 1.0);
    float out = sampler.processSample(0, 0.5f, 4.0, true, false, 0.0, rng, nd);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("ProcessSpec: BbdFeedback prepare overload", "[processspec]")
{
    patina::ProcessSpec spec{44100.0, 512, 2};
    BbdFeedback fm;
    fm.prepare(spec);
    std::minstd_rand rng(42);
    std::normal_distribution<double> nd(0.0, 1.0);
    float out = fm.process(0, 0.3f, true, rng, nd, 1.0, 1e-6, 44100.0, false, 1.0);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("ProcessSpec: ToneFilter prepare overload", "[processspec]")
{
    patina::ProcessSpec spec{44100.0, 512, 2};
    ToneFilter tf;
    tf.prepare(spec);
    float out = tf.processSample(0, 0.5f);
    REQUIRE(std::isfinite(out));
}

// ============================================================================
// Config/Params struct tests
// ============================================================================

TEST_CASE("CompanderModule::Config: setConfig/getConfig roundtrip", "[config]")
{
    CompanderModule cm;
    cm.prepare(2, 44100.0);

    CompanderModule::Config cfg;
    cfg.thresholdVrms = 0.5f;
    cfg.ratio = 3.0f;
    cfg.kneeDb = 8.0f;
    cfg.vcaOutputRatio = 0.7;
    cfg.preDeEmphasis = false;
    cm.setConfig(cfg);

    auto got = cm.getConfig();
    REQUIRE_THAT(got.thresholdVrms, WithinAbs(0.5f, 1e-6f));
    REQUIRE_THAT(got.ratio, WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(got.kneeDb, WithinAbs(8.0f, 1e-6f));
    REQUIRE_THAT(got.vcaOutputRatio, WithinAbs(0.7, 1e-9));
    REQUIRE(got.preDeEmphasis == false);
}

TEST_CASE("CompanderModule::Config: default is NE570", "[config]")
{
    CompanderModule::Config cfg;
    REQUIRE_THAT((double)cfg.thresholdVrms, WithinAbs(PartsConstants::V_ref_NE570, 1e-6));
    REQUIRE_THAT((double)cfg.ratio, WithinAbs(2.0, 1e-6));
    REQUIRE_THAT(cfg.vcaOutputRatio, WithinAbs(PartsConstants::ne570VcaOutputRatio, 1e-9));
}

TEST_CASE("DiodeClipper::Params: processBlock matches per-sample", "[config]")
{
    DiodeClipper dm1, dm2;
    dm1.prepare(1, 44100.0);
    dm2.prepare(1, 44100.0);

    DiodeClipper::Params params;
    params.drive = 0.7f;
    params.mode = 2; // Tanh

    constexpr int N = 64;
    float buf1[N], buf2[N];
    for (int i = 0; i < N; ++i) {
        buf1[i] = buf2[i] = 0.3f * std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);
    }

    // Per-sample processing
    for (int i = 0; i < N; ++i)
        buf1[i] = dm1.process(0, buf1[i], params.drive, params.mode);

    // Block processing
    float* io[] = {buf2};
    dm2.processBlock(io, 1, N, params);

    for (int i = 0; i < N; ++i) {
        REQUIRE_THAT(buf2[i], WithinAbs(buf1[i], 1e-7f));
    }
}

TEST_CASE("BbdStageEmulator::Params: processBlock matches per-frame", "[config]")
{
    BbdStageEmulator bbd1, bbd2;
    bbd1.prepare(2, 44100.0);
    bbd2.prepare(2, 44100.0);

    BbdStageEmulator::Params params;
    params.stages = 4096;
    params.supplyVoltage = 12.0;

    constexpr int N = 32;
    float ch0a[N], ch1a[N], ch0b[N], ch1b[N];
    for (int i = 0; i < N; ++i) {
        float v = 0.2f * std::sin(2.0f * 3.14159f * 1000.0f * i / 44100.0f);
        ch0a[i] = ch0b[i] = v;
        ch1a[i] = ch1b[i] = -v;
    }

    // Per-frame
    std::vector<float> frame(2);
    for (int i = 0; i < N; ++i) {
        frame[0] = ch0a[i];
        frame[1] = ch1a[i];
        bbd1.process(frame, 4.0, params.stages, params.supplyVoltage, params.enableAging, params.ageYears);
        ch0a[i] = frame[0];
        ch1a[i] = frame[1];
    }

    // processBlock
    float* io[] = {ch0b, ch1b};
    bbd2.processBlock(io, 2, N, 4.0, params);

    for (int i = 0; i < N; ++i) {
        REQUIRE_THAT(ch0b[i], WithinAbs(ch0a[i], 1e-7f));
        REQUIRE_THAT(ch1b[i], WithinAbs(ch1a[i], 1e-7f));
    }
}

TEST_CASE("BbdFeedback::Config: processBlock matches per-sample", "[config]")
{
    BbdFeedback fm1, fm2;
    fm1.prepare(1, 44100.0);
    fm2.prepare(1, 44100.0);

    BbdFeedback::Config config;
    config.emulateOpAmpSaturation = true;
    config.highVoltageMode = false;
    config.capacitanceScale = 1.0;
    config.opAmpNoiseGain = 1.0;
    config.bbdNoiseLevel = 0.0; // disable noise for deterministic test

    constexpr int N = 32;
    float buf1[N], buf2[N];
    for (int i = 0; i < N; ++i)
        buf1[i] = buf2[i] = 0.2f * std::sin(2.0f * 3.14159f * 500.0f * i / 44100.0f);

    // Per-sample
    std::minstd_rand rng1(42);
    std::normal_distribution<double> nd1(0.0, 1.0);
    for (int i = 0; i < N; ++i)
        buf1[i] = fm1.process(0, buf1[i], config.emulateOpAmpSaturation,
                              rng1, nd1, config.opAmpNoiseGain, config.bbdNoiseLevel,
                              44100.0, config.highVoltageMode, config.capacitanceScale);

    // processBlock
    std::minstd_rand rng2(42);
    std::normal_distribution<double> nd2(0.0, 1.0);
    float* io[] = {buf2};
    fm2.processBlock(io, 1, N, config, rng2, nd2);

    for (int i = 0; i < N; ++i)
        REQUIRE_THAT(buf2[i], WithinAbs(buf1[i], 1e-7f));
}

// ============================================================================
// processBlock tests (per-module)
// ============================================================================

TEST_CASE("OutputStage::processBlock processes correctly", "[processblock]")
{
    OutputStage om;
    om.prepare(2, 44100.0);
    om.setCutoffHz(4000.0);

    constexpr int N = 64;
    float ch0[N], ch1[N];
    for (int i = 0; i < N; ++i) {
        ch0[i] = 0.4f;
        ch1[i] = -0.4f;
    }

    float* io[] = {ch0, ch1};
    om.processBlock(io, 2, N, 9.0);

    // LPF should smooth toward DC value, output should be finite and attenuated
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(ch0[i]));
        REQUIRE(std::isfinite(ch1[i]));
    }
}

TEST_CASE("InputFilter::processBlock processes correctly", "[processblock]")
{
    InputFilter lpf;
    lpf.prepare(1, 44100.0);
    lpf.setCutoffHz(2000.0);

    constexpr int N = 64;
    float buf[N];
    for (int i = 0; i < N; ++i)
        buf[i] = (i % 2 == 0) ? 1.0f : -1.0f; // Nyquist signal

    float* io[] = {buf};
    lpf.processBlock(io, 1, N);

    // LPF should heavily attenuate Nyquist
    float maxAbs = 0.0f;
    for (int i = N / 2; i < N; ++i)
        maxAbs = std::max(maxAbs, std::fabs(buf[i]));
    REQUIRE(maxAbs < 0.5f); // Significant attenuation
}

TEST_CASE("InputBuffer::processBlock processes correctly", "[processblock]")
{
    InputBuffer buf;
    buf.prepare(1, 44100.0);

    constexpr int N = 32;
    float data[N];
    for (int i = 0; i < N; ++i)
        data[i] = 0.3f;

    float* io[] = {data};
    buf.processBlock(io, 1, N);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(data[i]));
}

TEST_CASE("ToneFilter::processBlock processes correctly", "[processblock]")
{
    ToneFilter tf;
    tf.prepare(2, 44100.0, 512);
    tf.setDefaultCutoff(3000.0f);

    constexpr int N = 64;
    float ch0[N], ch1[N];
    for (int i = 0; i < N; ++i) {
        ch0[i] = 0.5f * std::sin(2.0f * 3.14159f * 8000.0f * i / 44100.0f);
        ch1[i] = ch0[i];
    }

    float* io[] = {ch0, ch1};
    tf.processBlock(io, 2, N);

    // 8kHz should be attenuated by 3kHz LPF
    float maxAbs = 0.0f;
    for (int i = N / 2; i < N; ++i)
        maxAbs = std::max(maxAbs, std::fabs(ch0[i]));
    REQUIRE(maxAbs < 0.4f);
}

TEST_CASE("CompanderModule::processBlockCompress processes correctly", "[processblock]")
{
    CompanderModule cm;
    cm.prepare(1, 44100.0);

    constexpr int N = 128;
    float buf[N];
    for (int i = 0; i < N; ++i)
        buf[i] = 0.8f * std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);

    float* io[] = {buf};
    cm.processBlockCompress(io, 1, N, 0.8f);

    // Should compress - all output should be finite
    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}
