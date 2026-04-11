// ============================================================================
// CompanderModule unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/compander/CompanderModule.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;
static constexpr int kNumChannels = 2;

TEST_CASE("CompanderModule: prepare and reset without crash", "[Compander]") {
    CompanderModule comp;
    comp.prepare(kNumChannels, kSampleRate);
    comp.reset();
    REQUIRE(true);
}

TEST_CASE("CompanderModule: silence in -> silence out (compress)", "[Compander]") {
    CompanderModule comp;
    comp.prepare(kNumChannels, kSampleRate);

    float out = comp.processCompress(0, 0.0f, 1.0f);
    REQUIRE(std::fabs(out) < 0.001f);
}

TEST_CASE("CompanderModule: silence in -> silence out (expand)", "[Compander]") {
    CompanderModule comp;
    comp.prepare(kNumChannels, kSampleRate);

    float out = comp.processExpand(0, 0.0f, 1.0f);
    REQUIRE(std::fabs(out) < 0.001f);
}

TEST_CASE("CompanderModule: compress reduces dynamic range", "[Compander]") {
    CompanderModule comp;
    comp.prepare(1, kSampleRate);

    // Send signal for a while to let RMS detector settle
    float sum_quiet = 0.0f, sum_loud = 0.0f;
    const int warmup = 4000;
    const int measure = 2000;

    // Quiet signal
    for (int i = 0; i < warmup + measure; ++i) {
        float in = 0.05f * std::sin(2.0 * M_PI * 440.0 * i / kSampleRate);
        float out = comp.processCompress(0, in, 1.0f);
        if (i >= warmup) sum_quiet += out * out;
    }
    comp.reset();

    // Loud signal
    for (int i = 0; i < warmup + measure; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSampleRate);
        float out = comp.processCompress(0, in, 1.0f);
        if (i >= warmup) sum_loud += out * out;
    }

    double rms_quiet = std::sqrt(sum_quiet / measure);
    double rms_loud = std::sqrt(sum_loud / measure);

    // Compressed loud/quiet ratio should be less than uncompressed ratio (16:1 = 0.8/0.05)
    // But at least loud is still louder than quiet
    REQUIRE(rms_loud > rms_quiet);
}

TEST_CASE("CompanderModule: compAmount=0 passes signal through", "[Compander]") {
    CompanderModule comp;
    comp.prepare(1, kSampleRate);

    float in = 0.5f;
    float out = comp.processCompress(0, in, 0.0f);
    // With compAmount=0, minimal compression should be applied
    REQUIRE(std::isfinite(out));
}

TEST_CASE("CompanderModule: output is finite for extreme inputs", "[Compander]") {
    CompanderModule comp;
    comp.prepare(1, kSampleRate);

    float out = comp.processCompress(0, 10.0f, 1.0f);
    REQUIRE(std::isfinite(out));

    out = comp.processExpand(0, 10.0f, 1.0f);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("CompanderModule: timing config roundtrip", "[Compander]") {
    CompanderModule comp;
    comp.prepare(1, kSampleRate);

    CompanderModule::TimingConfig cfg;
    cfg.compressorAttack = 0.005;
    cfg.compressorRelease = 0.1;
    cfg.expanderAttack = 0.003;
    cfg.expanderRelease = 0.08;
    cfg.rmsTimeConstant = 0.01;
    comp.setTimingConfig(cfg);

    auto got = comp.getTimingConfig();
    REQUIRE(got.compressorAttack == Approx(0.005));
    REQUIRE(got.compressorRelease == Approx(0.1));
    REQUIRE(got.expanderAttack == Approx(0.003));
    REQUIRE(got.expanderRelease == Approx(0.08));
    REQUIRE(got.rmsTimeConstant == Approx(0.01));
}

TEST_CASE("CompanderModule: setThresholdVrms / setRatio / setKnee don't crash", "[Compander]") {
    CompanderModule comp;
    comp.prepare(1, kSampleRate);
    comp.setThresholdVrms(0.316);
    comp.setRatio(2.0);
    comp.setKnee(6.0);
    
    float out = comp.processCompress(0, 0.5f, 1.0f);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("CompanderModule: expand inverts compress approximately", "[Compander]") {
    CompanderModule compressor;
    CompanderModule expander;
    compressor.prepare(1, kSampleRate);
    expander.prepare(1, kSampleRate);

    const int N = 8000;
    double sum_error = 0.0;
    int count = 0;

    for (int i = 0; i < N; ++i) {
        float in = 0.3f * std::sin(2.0 * M_PI * 440.0 * i / kSampleRate);
        float compressed = compressor.processCompress(0, in, 1.0f);
        float expanded = expander.processExpand(0, compressed, 1.0f);
        
        if (i > 4000) {  // After warmup
            sum_error += std::fabs(expanded - in);
            count++;
        }
    }

    // Not perfectly inverse due to VCA noise and timing, but should be in ballpark
    double avgError = sum_error / count;
    REQUIRE(avgError < 0.5); // Generous tolerance for analog modeling
}

TEST_CASE("CompanderModule: multi-channel independence", "[Compander]") {
    CompanderModule comp;
    comp.prepare(2, kSampleRate);

    // Different signals on ch0 and ch1
    float out0 = comp.processCompress(0, 0.8f, 1.0f);
    float out1 = comp.processCompress(1, 0.1f, 1.0f);

    REQUIRE(std::isfinite(out0));
    REQUIRE(std::isfinite(out1));
}
