#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/config/Presets.h"
#include "dsp/circuits/bbd/BbdFeedback.h"

using namespace Catch::Matchers;
using namespace patina::presets;

// ============================================================================
// Compander preset tests
// ============================================================================

TEST_CASE("Presets: ne570 defaults", "[presets]")
{
    auto cfg = ne570();
    REQUIRE_THAT(cfg.vcaOutputRatio, WithinAbs(0.60, 1e-6));
    REQUIRE(cfg.timing.compressorAttack > 0.0);
    REQUIRE(cfg.timing.compressorRelease > 0.0);
}

TEST_CASE("Presets: sa571n faster timing", "[presets]")
{
    auto ne = ne570();
    auto sa = sa571n();
    // SA571N should have faster attack and release
    REQUIRE(sa.timing.compressorAttack < ne.timing.compressorAttack);
    REQUIRE(sa.timing.compressorRelease < ne.timing.compressorRelease);
    REQUIRE(sa.vcaOutputRatio > ne.vcaOutputRatio);
}

TEST_CASE("Presets: compander configs are usable", "[presets]")
{
    CompanderModule cm;
    cm.prepare(2, 44100.0);

    // Apply NE570 preset
    cm.setConfig(ne570());
    float out = cm.processCompress(0, 0.5f, 0.8f);
    REQUIRE(std::isfinite(out));

    // Apply SA571N preset and process
    cm.setConfig(sa571n());
    out = cm.processCompress(0, 0.5f, 0.8f);
    REQUIRE(std::isfinite(out));
}

// ============================================================================
// Op-amp preset tests
// ============================================================================

TEST_CASE("Presets: opAmp presets have different characteristics", "[presets]")
{
    auto tl  = tl072cp();
    auto jrc = jrc4558d();
    auto opa = opa2134();
    auto lm  = lm4562();
    auto ne  = ne5532();

    // JRC4558D should have much lower slew rate
    REQUIRE(jrc.slewRate < tl.slewRate);

    // LM4562 should have highest saturation threshold (cleanest)
    REQUIRE(lm.satThreshold18V > tl.satThreshold18V);
    REQUIRE(lm.satThreshold18V > jrc.satThreshold18V);

    // OPA2134 should be cleaner than TL072
    REQUIRE(opa.satThreshold18V > tl.satThreshold18V);
}

TEST_CASE("Presets: opAmpFromType matches named presets", "[presets]")
{
    auto fromType = opAmpFromType(ModdingConfig::JRC4558D);
    auto named = jrc4558d();
    REQUIRE_THAT(fromType.slewRate, WithinAbs(named.slewRate, 1e-6));
    REQUIRE_THAT(fromType.satThreshold18V, WithinAbs(named.satThreshold18V, 1e-9));
}

TEST_CASE("Presets: opAmp presets are usable", "[presets]")
{
    BbdFeedback fm;
    fm.prepare(2, 44100.0);

    fm.setOpAmpOverrides(jrc4558d());
    std::minstd_rand rng(42);
    std::normal_distribution<double> nd(0.0, 1.0);
    float out = fm.process(0, 0.3f, true, rng, nd, 1.0, 1e-6, 44100.0, false, 1.0);
    REQUIRE(std::isfinite(out));
}

// ============================================================================
// BBD chip preset tests
// ============================================================================

TEST_CASE("Presets: mn3005 stages", "[presets]")
{
    auto p = mn3005();
    REQUIRE(p.stages == 4096);
    REQUIRE_THAT(p.supplyVoltage, WithinAbs(15.0, 1e-6));
}

TEST_CASE("Presets: mn3207 stages", "[presets]")
{
    auto p = mn3207();
    REQUIRE(p.stages == 1024);
    REQUIRE_THAT(p.supplyVoltage, WithinAbs(9.0, 1e-6));
}

TEST_CASE("Presets: mn3005Dual stages", "[presets]")
{
    auto p = mn3005Dual();
    REQUIRE(p.stages == PartsConstants::bbdStagesDefault);
}

TEST_CASE("Presets: BBD presets are usable", "[presets]")
{
    BbdStageEmulator bbd;
    bbd.prepare(2, 44100.0);

    auto params = mn3005();
    std::vector<float> frame = {0.3f, -0.3f};
    bbd.process(frame, 4.0, params.stages, params.supplyVoltage, params.enableAging, params.ageYears);
    REQUIRE(std::isfinite(frame[0]));
    REQUIRE(std::isfinite(frame[1]));
}

// ============================================================================
// CapGrade preset tests
// ============================================================================

TEST_CASE("Presets: capGrade bandwidth scaling", "[presets]")
{
    double stock = capGradeBandwidthScale(ModdingConfig::Standard);
    double film  = capGradeBandwidthScale(ModdingConfig::Film);
    double audio = capGradeBandwidthScale(ModdingConfig::AudioGrade);

    // Higher grade = lower ESR = wider bandwidth
    REQUIRE(film > stock);
    REQUIRE(audio > film);

    // Stock should be 1.0 (sqrt(1/1.0))
    REQUIRE_THAT(stock, WithinAbs(1.0, 1e-6));
}
