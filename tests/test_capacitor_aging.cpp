// ============================================================================
// CapacitorAging unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/power/CapacitorAging.h"
#include "dsp/config/ModdingConfig.h"

using Catch::Approx;

TEST_CASE("CapAging: default params -> no aging", "[CapAging]") {
    CapacitorAging ca;
    // ageYears=0 -> capacitanceScale=1.0, esrScale=1.0
    REQUIRE(ca.getCapacitanceScale() == Approx(1.0));
    REQUIRE(ca.getEsrScale() == Approx(1.0));
    REQUIRE(ca.getMicrophonicsScale() == Approx(1.0));
}

TEST_CASE("CapAging: aging decreases capacitance", "[CapAging]") {
    CapacitorAging ca;
    ca.setAge(0.0f);
    double fresh = ca.getCapacitanceScale();

    ca.setAge(20.0f);
    double aged = ca.getCapacitanceScale();

    REQUIRE(aged < fresh);
    REQUIRE(aged >= PartsConstants::aging_min_cap_scale);  // Do not fall below minimum
}

TEST_CASE("CapAging: aging increases ESR", "[CapAging]") {
    CapacitorAging ca;
    ca.setAge(0.0f);
    double freshEsr = ca.getEsrScale();

    ca.setAge(20.0f);
    double agedEsr = ca.getEsrScale();

    REQUIRE(agedEsr > freshEsr);
    REQUIRE(agedEsr <= 5.0);  // Upper-bound check
}

TEST_CASE("CapAging: aging increases microphonics", "[CapAging]") {
    CapacitorAging ca;
    ca.setAge(0.0f);
    double freshMic = ca.getMicrophonicsScale();

    ca.setAge(20.0f);
    double agedMic = ca.getMicrophonicsScale();

    REQUIRE(agedMic > freshMic);
    REQUIRE(agedMic <= 3.0);  // Upper-bound check
}

TEST_CASE("CapAging: temperature accelerates aging", "[CapAging]") {
    CapacitorAging ca_cool, ca_hot;
    ca_cool.setAge(10.0f);
    ca_cool.setTemperature(25.0f);

    ca_hot.setAge(10.0f);
    ca_hot.setTemperature(45.0f);

    // Higher temperature causes greater capacitance loss
    REQUIRE(ca_hot.getCapacitanceScale() < ca_cool.getCapacitanceScale());
    // Higher temperature causes larger ESR increase
    REQUIRE(ca_hot.getEsrScale() > ca_cool.getEsrScale());
}

TEST_CASE("CapAging: qualityFactor slows aging", "[CapAging]") {
    CapacitorAging ca_cheap, ca_premium;
    ca_cheap.setAge(15.0f);
    ca_cheap.setQualityFactor(0.5f);

    ca_premium.setAge(15.0f);
    ca_premium.setQualityFactor(2.0f);

    // Higher-quality retains capacitance better
    REQUIRE(ca_premium.getCapacitanceScale() > ca_cheap.getCapacitanceScale());
    // Higher-quality shows less ESR increase
    REQUIRE(ca_premium.getEsrScale() < ca_cheap.getEsrScale());
}

TEST_CASE("CapAging: CapGrade interaction - Standard vs AudioGrade", "[CapAging]") {
    CapacitorAging ca;
    ca.setAge(20.0f);

    const auto& stdGrade = ModdingConfig::capGradeSpecs[ModdingConfig::Standard];
    const auto& audioGrade = ModdingConfig::capGradeSpecs[ModdingConfig::AudioGrade];

    double capStd = ca.getCapacitanceScale(stdGrade);
    double capAudio = ca.getCapacitanceScale(audioGrade);

    // AudioGrade is more resistant to aging
    REQUIRE(capAudio > capStd);
}

TEST_CASE("CapAging: ESR with CapGrade reflects base ESR", "[CapAging]") {
    CapacitorAging ca;
    ca.setAge(10.0f);

    const auto& stdGrade = ModdingConfig::capGradeSpecs[ModdingConfig::Standard];
    const auto& filmGrade = ModdingConfig::capGradeSpecs[ModdingConfig::Film];

    double esrStd = ca.getEsrScale(stdGrade);
    double esrFilm = ca.getEsrScale(filmGrade);

    // Film (base esr=0.45) x aging is smaller than Standard (base esr=1.0) x aging
    REQUIRE(esrFilm < esrStd);
}

TEST_CASE("CapAging: getAgedSpec produces valid spec", "[CapAging]") {
    CapacitorAging ca;
    ca.setAge(15.0f);

    const auto& base = ModdingConfig::capGradeSpecs[ModdingConfig::Film];
    auto aged = ca.getAgedSpec(base);

    // toleranceScale increases with aging (wider variance)
    REQUIRE(aged.toleranceScale >= base.toleranceScale);
    // ESR increases with aging
    REQUIRE(aged.esr >= base.esr);
    // 
    REQUIRE(aged.microphonicsScale >= base.microphonicsScale);
}

TEST_CASE("CapAging: effectiveAge increases with temperature", "[CapAging]") {
    CapacitorAging ca;
    ca.setAge(10.0f);

    ca.setTemperature(25.0f);
    double effAge25 = ca.getEffectiveAge();

    ca.setTemperature(35.0f);
    double effAge35 = ca.getEffectiveAge();

    // 10degC  -> 2
    REQUIRE(effAge35 == Approx(effAge25 * 2.0).epsilon(0.01));
}

TEST_CASE("CapAging: humidity accelerates aging", "[CapAging]") {
    CapacitorAging ca_dry, ca_wet;
    ca_dry.setAge(10.0f);
    ca_dry.setHumidityFactor(1.0f);

    ca_wet.setAge(10.0f);
    ca_wet.setHumidityFactor(1.5f);

    REQUIRE(ca_wet.getCapacitanceScale() < ca_dry.getCapacitanceScale());
}

TEST_CASE("CapAging: getSummary returns consistent values", "[CapAging]") {
    CapacitorAging ca;
    ca.setAge(5.0f);
    ca.setTemperature(30.0f);

    auto summary = ca.getSummary();
    REQUIRE(summary.effectiveAge > 5.0);  // 
    REQUIRE(summary.capacitanceScale > 0.0);
    REQUIRE(summary.capacitanceScale <= 1.0);
    REQUIRE(summary.esrScale >= 1.0);
    REQUIRE(summary.microphonicsScale >= 1.0);
}

TEST_CASE("CapAging: getSummary with CapGrade", "[CapAging]") {
    CapacitorAging ca;
    ca.setAge(10.0f);

    const auto& grade = ModdingConfig::capGradeSpecs[ModdingConfig::AudioGrade];
    auto summary = ca.getSummary(grade);

    REQUIRE(summary.capacitanceScale > 0.0);
    REQUIRE(summary.capacitanceScale <= 1.0);
    REQUIRE(summary.esrScale > 0.0);  // AudioGrade base esr=0.20 x aging
}

TEST_CASE("CapAging: capacitanceScale never below minimum", "[CapAging]") {
    CapacitorAging ca;
    ca.setAge(100.0f);  // 
    ca.setTemperature(60.0f);
    ca.setQualityFactor(0.1f);
    ca.setHumidityFactor(2.0f);

    double scale = ca.getCapacitanceScale();
    REQUIRE(scale >= PartsConstants::aging_min_cap_scale);
}

TEST_CASE("CapAging: setParams updates all values", "[CapAging]") {
    CapacitorAging ca;
    CapacitorAging::Params p;
    p.ageYears = 25.0f;
    p.temperature = 40.0f;
    p.qualityFactor = 0.8f;
    p.humidityFactor = 1.3f;
    ca.setParams(p);

    auto summary = ca.getSummary();
    REQUIRE(summary.effectiveAge > 25.0);  // +
    REQUIRE(summary.capacitanceScale < 1.0);
}
