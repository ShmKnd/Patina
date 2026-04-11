// ============================================================================
// ModdingConfig unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "dsp/config/ModdingConfig.h"

TEST_CASE("ModdingConfig: default values", "[ModdingConfig]") {
    ModdingConfig cfg;
    REQUIRE(cfg.opAmp == ModdingConfig::TL072CP);
    REQUIRE(cfg.compander == ModdingConfig::NE570);
    REQUIRE(cfg.capGrade == ModdingConfig::Standard);
}

TEST_CASE("ModdingConfig: getOpAmpSpec returns valid spec", "[ModdingConfig]") {
    ModdingConfig cfg;
    cfg.opAmp = ModdingConfig::JRC4558D;
    auto& spec = cfg.getOpAmpSpec();
    REQUIRE(std::string(spec.name) == "JRC4558D");
    REQUIRE(spec.slewRate > 0.0);
}

TEST_CASE("ModdingConfig: getCompanderSpec returns valid spec", "[ModdingConfig]") {
    ModdingConfig cfg;
    cfg.compander = ModdingConfig::SA571N;
    auto& spec = cfg.getCompanderSpec();
    REQUIRE(std::string(spec.name) == "SA571N");
    REQUIRE(spec.attackScale > 0.0);
}

TEST_CASE("ModdingConfig: getCapGradeSpec returns valid spec", "[ModdingConfig]") {
    ModdingConfig cfg;
    cfg.capGrade = ModdingConfig::AudioGrade;
    auto& spec = cfg.getCapGradeSpec();
    REQUIRE(std::string(spec.name) == "Audio Grade");
    REQUIRE(spec.esr > 0.0);
}

TEST_CASE("ModdingConfig: all OpAmp specs are accessible", "[ModdingConfig]") {
    ModdingConfig cfg;
    for (int i = 0; i < ModdingConfig::kOpAmpCount; ++i) {
        cfg.opAmp = i;
        auto& spec = cfg.getOpAmpSpec();
        REQUIRE(spec.name != nullptr);
        REQUIRE(spec.slewRate > 0.0);
        REQUIRE(spec.satThreshold18V > 0.0);
        REQUIRE(spec.satThreshold9V > 0.0);
    }
}

TEST_CASE("ModdingConfig: all CompanderType specs are accessible", "[ModdingConfig]") {
    ModdingConfig cfg;
    for (int i = 0; i < ModdingConfig::kCompanderCount; ++i) {
        cfg.compander = i;
        auto& spec = cfg.getCompanderSpec();
        REQUIRE(spec.name != nullptr);
        REQUIRE(spec.vcaOutputRatio > 0.0);
    }
}

TEST_CASE("ModdingConfig: all CapGrade specs are accessible", "[ModdingConfig]") {
    ModdingConfig cfg;
    for (int i = 0; i < ModdingConfig::kCapGradeCount; ++i) {
        cfg.capGrade = i;
        auto& spec = cfg.getCapGradeSpec();
        REQUIRE(spec.name != nullptr);
    }
}

TEST_CASE("ModdingConfig: out-of-range opAmp clamps", "[ModdingConfig]") {
    ModdingConfig cfg;
    cfg.opAmp = 99;
    auto& spec = cfg.getOpAmpSpec();
    REQUIRE(spec.name != nullptr); // Should clamp, not crash
}

TEST_CASE("ModdingConfig: toJson / fromJson roundtrip", "[ModdingConfig]") {
    ModdingConfig original;
    original.opAmp = ModdingConfig::OPA2134;
    original.compander = ModdingConfig::SA571N;
    original.capGrade = ModdingConfig::Film;

    std::string json = original.toJson();
    ModdingConfig restored = ModdingConfig::fromJson(json);

    REQUIRE(original == restored);
}

TEST_CASE("ModdingConfig: fromJson with default values on missing keys", "[ModdingConfig]") {
    ModdingConfig cfg = ModdingConfig::fromJson("{}");
    REQUIRE(cfg.opAmp == ModdingConfig::TL072CP);
    REQUIRE(cfg.compander == ModdingConfig::NE570);
    REQUIRE(cfg.capGrade == ModdingConfig::Standard);
}

TEST_CASE("ModdingConfig: fromJson clamps out-of-range values", "[ModdingConfig]") {
    ModdingConfig cfg = ModdingConfig::fromJson("{\"opAmp\":99,\"compander\":-1,\"capGrade\":100}");
    REQUIRE(cfg.opAmp >= 0);
    REQUIRE(cfg.opAmp < ModdingConfig::kOpAmpCount);
    REQUIRE(cfg.compander >= 0);
    REQUIRE(cfg.compander < ModdingConfig::kCompanderCount);
    REQUIRE(cfg.capGrade >= 0);
    REQUIRE(cfg.capGrade < ModdingConfig::kCapGradeCount);
}

TEST_CASE("ModdingConfig: equality comparison", "[ModdingConfig]") {
    ModdingConfig a, b;
    REQUIRE(a == b);

    b.opAmp = ModdingConfig::NE5532;
    REQUIRE(a != b);
}
