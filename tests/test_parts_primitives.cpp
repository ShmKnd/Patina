#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "dsp/parts/DiodePrimitive.h"
#include "dsp/parts/TransformerPrimitive.h"
#include "dsp/parts/TapePrimitive.h"
#include "dsp/parts/TubeTriode.h"
#include "dsp/parts/PhotocellPrimitive.h"
#include "dsp/parts/OTA_Primitive.h"
#include "dsp/parts/JFET_Primitive.h"
#include "dsp/parts/BJT_Primitive.h"
#include "dsp/parts/RC_Element.h"

using Catch::Matchers::WithinAbs;

// ============================================================================
// DiodePrimitive
// ============================================================================

TEST_CASE("DiodePrimitive: all presets construct with valid Vf", "[Parts][Diode]") {
    auto check = [](const DiodePrimitive::Spec& s, const char* name) {
        DiodePrimitive d(s);
        INFO("Preset: " << name);
        REQUIRE(d.getSpec().Vf_25C > 0.0);
        REQUIRE(d.getSpec().Vf_25C < 2.0);
    };
    check(DiodePrimitive::Si1N4148(),     "Si1N4148");
    check(DiodePrimitive::Schottky1N5818(),"Schottky1N5818");
    check(DiodePrimitive::GeOA91(),        "GeOA91");
    check(DiodePrimitive::LowVfSilicon(),  "LowVfSilicon");
    check(DiodePrimitive::OtaInputDiode(), "OtaInputDiode");
}

TEST_CASE("DiodePrimitive: effectiveVf decreases with temperature", "[Parts][Diode]") {
    DiodePrimitive d(DiodePrimitive::Si1N4148());
    double vfCold = d.effectiveVf(0.0);
    double vf25   = d.effectiveVf(25.0);
    double vfHot  = d.effectiveVf(60.0);
    REQUIRE(vfCold > vf25);
    REQUIRE(vf25   > vfHot);
}

TEST_CASE("DiodePrimitive: saturate is odd-symmetric for symmetric diode", "[Parts][Diode]") {
    DiodePrimitive d(DiodePrimitive::Si1N4148());
    // Si1N4148 has asymmetry = 0.0
    double pos = d.saturate(0.5);
    double neg = d.saturate(-0.5);
    REQUIRE_THAT(pos, WithinAbs(-neg, 1e-10));
}

TEST_CASE("DiodePrimitive: clip limits signal", "[Parts][Diode]") {
    DiodePrimitive d(DiodePrimitive::Si1N4148());
    double big = d.clip(10.0);
    REQUIRE(std::fabs(big) < 10.0);
    REQUIRE(std::fabs(big) > 0.0);
}

TEST_CASE("DiodePrimitive: zero input -> zero output", "[Parts][Diode]") {
    DiodePrimitive d(DiodePrimitive::GeOA91());
    REQUIRE_THAT(d.saturate(0.0), WithinAbs(0.0, 1e-15));
    REQUIRE_THAT(d.clip(0.0),     WithinAbs(0.0, 1e-15));
}

TEST_CASE("DiodePrimitive: junction capacitance is positive", "[Parts][Diode]") {
    DiodePrimitive d(DiodePrimitive::Si1N4148());
    REQUIRE(d.junctionCapacitance(0.0) > 0.0);
    REQUIRE(d.junctionCapacitance(0.3) > 0.0);
}

// ============================================================================
// TransformerPrimitive
// ============================================================================

TEST_CASE("TransformerPrimitive: all presets have valid specs", "[Parts][Transformer]") {
    auto check = [](const TransformerPrimitive::Spec& s, const char* name) {
        TransformerPrimitive t(s);
        INFO("Preset: " << name);
        REQUIRE(t.getSpec().windingR > 0.0);
        REQUIRE(t.getSpec().leakageL > 0.0);
        REQUIRE(t.getSpec().noiseLevel > 0.0);
    };
    check(TransformerPrimitive::BritishConsole(),  "BritishConsole");
    check(TransformerPrimitive::AmericanConsole(), "AmericanConsole");
    check(TransformerPrimitive::CompactFetOutput(),"CompactFetOutput");
    check(TransformerPrimitive::Neve1073Input(),   "Neve1073Input");
    check(TransformerPrimitive::API2520Output(),   "API2520Output");
    check(TransformerPrimitive::Lundahl1538(),     "Lundahl1538");
    check(TransformerPrimitive::JensenDIBox(),     "JensenDIBox");
    check(TransformerPrimitive::InterstageTriode(),"InterstageTriode");
    check(TransformerPrimitive::RibbonMicOutput(), "RibbonMicOutput");
}

TEST_CASE("TransformerPrimitive: saturation limits signal", "[Parts][Transformer]") {
    TransformerPrimitive t(TransformerPrimitive::BritishConsole());
    t.prepare(44100.0);
    double out = t.processCoreSaturation(10.0, 0.5);
    REQUIRE(std::fabs(out) < 10.0);
    REQUIRE(std::fabs(out) > 0.0);
}

TEST_CASE("TransformerPrimitive: zero input -> zero saturation output", "[Parts][Transformer]") {
    TransformerPrimitive t(TransformerPrimitive::AmericanConsole());
    t.prepare(44100.0);
    REQUIRE_THAT(t.processCoreSaturation(0.0, 0.5), WithinAbs(0.0, 1e-15));
}

TEST_CASE("TransformerPrimitive: muScale varies with temperature", "[Parts][Transformer]") {
    TransformerPrimitive t(TransformerPrimitive::BritishConsole());
    double mu25 = t.muScale(25.0);
    double mu60 = t.muScale(60.0);
    REQUIRE(mu25 != mu60);
    REQUIRE_THAT(mu25, WithinAbs(1.0, 0.01));
}

// ============================================================================
// TapePrimitive
// ============================================================================

TEST_CASE("TapePrimitive: all presets have valid specs", "[Parts][Tape]") {
    auto check = [](const TapePrimitive::Spec& s, const char* name) {
        TapePrimitive t(s);
        INFO("Preset: " << name);
        REQUIRE(t.getSpec().gapWidthNew > 0.0);
        REQUIRE(t.getSpec().baseHissLevel > 0.0);
    };
    check(TapePrimitive::HighSpeedDeck(), "HighSpeedDeck");
    check(TapePrimitive::MasteringDeck(), "MasteringDeck");
}

TEST_CASE("TapePrimitive: hysteresis saturates large signal", "[Parts][Tape]") {
    TapePrimitive t(TapePrimitive::HighSpeedDeck());
    double small_out = t.processHysteresis(0.1, 0.5);
    double big_out   = t.processHysteresis(10.0, 0.5);
    REQUIRE(std::fabs(big_out) < 10.0);
    REQUIRE(std::fabs(big_out) > std::fabs(small_out));
}

TEST_CASE("TapePrimitive: gapLossFc increases with tape speed", "[Parts][Tape]") {
    TapePrimitive t(TapePrimitive::HighSpeedDeck());
    // tapeSpeed is a multiplier clamped to [0.5, 2.0], base = 15 ips
    double fcSlow = t.gapLossFc(0.5, 0.0);  // 7.5 ips
    double fcFast = t.gapLossFc(2.0, 0.0);  // 30 ips
    REQUIRE(fcFast > fcSlow);
    REQUIRE(fcSlow > 0.0);
}

TEST_CASE("TapePrimitive: gapLossFc decreases with head wear", "[Parts][Tape]") {
    TapePrimitive t(TapePrimitive::HighSpeedDeck());
    double fc_new  = t.gapLossFc(15.0, 0.0);
    double fc_worn = t.gapLossFc(15.0, 0.8);
    REQUIRE(fc_new >= fc_worn);
}

// ============================================================================
// TubeTriode
// ============================================================================

TEST_CASE("TubeTriode: all presets have valid specs", "[Parts][Tube]") {
    auto check = [](const TubeTriode::Spec& s, const char* name) {
        TubeTriode t(s);
        INFO("Preset: " << name);
        REQUIRE(t.getSpec().plateR > 0.0);
        REQUIRE(t.getSpec().millerCap > 0.0);
    };
    check(TubeTriode::T12AX7(), "12AX7");
    check(TubeTriode::T12AT7(), "12AT7");
    check(TubeTriode::T12BH7(), "12BH7");
}

TEST_CASE("TubeTriode: transferFunction saturates", "[Parts][Tube]") {
    double small_out = TubeTriode::transferFunction(0.1);
    double big_out   = TubeTriode::transferFunction(5.0);
    REQUIRE(std::fabs(big_out) < 5.0);
    REQUIRE(std::fabs(big_out) > std::fabs(small_out));
}

TEST_CASE("TubeTriode: transferFunction zero -> zero", "[Parts][Tube]") {
    REQUIRE_THAT(TubeTriode::transferFunction(0.0), WithinAbs(0.0, 1e-15));
}

TEST_CASE("TubeTriode: pushPullBalance is odd function", "[Parts][Tube]") {
    double pos = TubeTriode::pushPullBalance(0.5);
    double neg = TubeTriode::pushPullBalance(-0.5);
    REQUIRE_THAT(pos + neg, WithinAbs(0.0, 1e-10));
}

TEST_CASE("TubeTriode: variableMuGain decreases with control voltage", "[Parts][Tube]") {
    auto vmu = TubeTriode::Tube6386();
    double g0 = TubeTriode::variableMuGain(0.0, vmu);
    double g5 = TubeTriode::variableMuGain(5.0, vmu);
    REQUIRE(g0 > g5);
    REQUIRE(g5 >= vmu.minGain);
}

TEST_CASE("TubeTriode: Miller filter processes without crash", "[Parts][Tube]") {
    TubeTriode t(TubeTriode::T12AX7());
    t.prepare(44100.0);
    for (int i = 0; i < 100; ++i) {
        double x = std::sin(2.0 * M_PI * 1000.0 * i / 44100.0);
        double out = t.processMiller(x);
        REQUIRE(std::isfinite(out));
    }
}

// ============================================================================
// PhotocellPrimitive
// ============================================================================

TEST_CASE("PhotocellPrimitive: all presets have valid specs", "[Parts][Photocell]") {
    auto check = [](const PhotocellPrimitive::Spec& s, const char* name) {
        PhotocellPrimitive p(s);
        INFO("Preset: " << name);
        REQUIRE(p.getSpec().elAttackMs > 0.0);
        REQUIRE(p.getSpec().cdsReleaseMaxMs > p.getSpec().cdsReleaseMinMs);
    };
    check(PhotocellPrimitive::T4B(),    "T4B");
    check(PhotocellPrimitive::VTL5C3(), "VTL5C3");
}

TEST_CASE("PhotocellPrimitive: output starts near 0 for zero sidechain", "[Parts][Photocell]") {
    PhotocellPrimitive p(PhotocellPrimitive::T4B());
    p.prepare(44100.0);
    // process returns cdsResistanceInv: 0 = no attenuation, 1 = full reduction
    double out = p.process(0.0);
    REQUIRE(out >= 0.0);
    REQUIRE(out < 0.1);
}

TEST_CASE("PhotocellPrimitive: output increases with sidechain level", "[Parts][Photocell]") {
    PhotocellPrimitive p(PhotocellPrimitive::T4B());
    p.prepare(44100.0);
    // Feed silence for a bit
    for (int i = 0; i < 1000; ++i) p.process(0.0);
    double outQuiet = p.process(0.0);

    // Now feed loud sidechain
    for (int i = 0; i < 4000; ++i) p.process(1.0);
    double outLoud = p.process(1.0);

    REQUIRE(outLoud > outQuiet);
}

// ============================================================================
// OTA_Primitive
// ============================================================================

TEST_CASE("OTA_Primitive: all presets have valid specs", "[Parts][OTA]") {
    auto check = [](const OTA_Primitive::Spec& s, const char* name) {
        OTA_Primitive o(s);
        INFO("Preset: " << name);
        REQUIRE(o.getSpec().saturationV > 0.0);
    };
    check(OTA_Primitive::LM13700(), "LM13700");
    check(OTA_Primitive::CA3080(),  "CA3080");
}

TEST_CASE("OTA_Primitive: saturate is odd-symmetric", "[Parts][OTA]") {
    OTA_Primitive o(OTA_Primitive::LM13700());
    double pos = o.saturate(0.5);
    double neg = o.saturate(-0.5);
    REQUIRE_THAT(pos, WithinAbs(-neg, 1e-10));
}

TEST_CASE("OTA_Primitive: saturate clips large signal", "[Parts][OTA]") {
    OTA_Primitive o(OTA_Primitive::LM13700());
    double out = o.saturate(100.0);
    REQUIRE(std::fabs(out) < 100.0);
}

TEST_CASE("OTA_Primitive: gmScale varies with temperature", "[Parts][OTA]") {
    OTA_Primitive o(OTA_Primitive::LM13700());
    double gm25 = o.gmScale(25.0);
    double gm60 = o.gmScale(60.0);
    REQUIRE_THAT(gm25, WithinAbs(1.0, 0.01));
    REQUIRE(gm25 != gm60);
}

TEST_CASE("OTA_Primitive: integrate accumulates", "[Parts][OTA]") {
    OTA_Primitive o(OTA_Primitive::LM13700());
    double state = 0.0;
    state = o.integrate(1.0, state, 0.1);
    REQUIRE(state > 0.0);
    double prev = state;
    state = o.integrate(1.0, state, 0.1);
    REQUIRE(state > prev);
}

TEST_CASE("OTA_Primitive: mismatch is near unity", "[Parts][OTA]") {
    OTA_Primitive o(OTA_Primitive::LM13700());
    // mismatch is 1.0 +/- small random offset (sigma ~0.015)
    REQUIRE(std::fabs(o.getMismatch() - 1.0) < 0.2);
}

// ============================================================================
// JFET_Primitive
// ============================================================================

TEST_CASE("JFET_Primitive: all presets have valid specs", "[Parts][JFET]") {
    auto check = [](const JFET_Primitive::Spec& s, const char* name) {
        JFET_Primitive j(s);
        INFO("Preset: " << name);
        REQUIRE(j.getSpec().Vp < 0.0);
    };
    check(JFET_Primitive::N2N5457(), "2N5457");
    check(JFET_Primitive::N2N3819(), "2N3819");
}

TEST_CASE("JFET_Primitive: softClip saturates", "[Parts][JFET]") {
    JFET_Primitive j(JFET_Primitive::N2N5457());
    double out = j.softClip(10.0);
    REQUIRE(std::fabs(out) < 10.0);
    REQUIRE(std::fabs(out) > 0.0);
}

TEST_CASE("JFET_Primitive: softClip zero -> zero", "[Parts][JFET]") {
    JFET_Primitive j(JFET_Primitive::N2N5457());
    REQUIRE_THAT(j.softClip(0.0), WithinAbs(0.0, 1e-15));
}

TEST_CASE("JFET_Primitive: vcaNonlinearity at gain 1 ~ input for small signal", "[Parts][JFET]") {
    JFET_Primitive j(JFET_Primitive::N2N5457());
    double out = j.vcaNonlinearity(0.01, 1.0);
    REQUIRE_THAT(out, WithinAbs(0.01, 0.005));
}

TEST_CASE("JFET_Primitive: tempFreqScale near 1 at 25degC", "[Parts][JFET]") {
    JFET_Primitive j(JFET_Primitive::N2N5457());
    REQUIRE_THAT(j.tempFreqScale(25.0), WithinAbs(1.0, 0.01));
}

// ============================================================================
// BJT_Primitive
// ============================================================================

TEST_CASE("BJT_Primitive: all presets have valid specs", "[Parts][BJT]") {
    auto check = [](const BJT_Primitive::Spec& s, const char* name) {
        BJT_Primitive b(s);
        INFO("Preset: " << name);
        REQUIRE(b.getSpec().mismatchSigma > 0.0);
        REQUIRE(b.getSpec().thermalNoise > 0.0);
    };
    check(BJT_Primitive::Generic(), "Generic");
    check(BJT_Primitive::Matched(), "Matched");
}

TEST_CASE("BJT_Primitive: Matched has lower mismatch than Generic", "[Parts][BJT]") {
    REQUIRE(BJT_Primitive::Matched().mismatchSigma < BJT_Primitive::Generic().mismatchSigma);
}

TEST_CASE("BJT_Primitive: saturate is bounded", "[Parts][BJT]") {
    double out = BJT_Primitive::saturate(10.0);
    REQUIRE(std::fabs(out) < 10.0);
    REQUIRE(std::fabs(out) > 0.0);
}

TEST_CASE("BJT_Primitive: saturate zero -> zero", "[Parts][BJT]") {
    REQUIRE_THAT(BJT_Primitive::saturate(0.0), WithinAbs(0.0, 1e-15));
}

TEST_CASE("BJT_Primitive: integrate accumulates", "[Parts][BJT]") {
    BJT_Primitive b(BJT_Primitive::Generic());
    double state = 0.0;
    state = b.integrate(1.0, state, 0.1);
    REQUIRE(state > 0.0);
}

TEST_CASE("BJT_Primitive: tempScale near 1 at 25degC", "[Parts][BJT]") {
    BJT_Primitive b(BJT_Primitive::Generic());
    REQUIRE_THAT(b.tempScale(25.0), WithinAbs(1.0, 0.01));
}

// ============================================================================
// RC_Element
// ============================================================================

TEST_CASE("RC_Element: cutoffHz matches 1/(2piRC)", "[Parts][RC]") {
    double R = 1000.0;
    double C = 100e-9;
    RC_Element rc(R, C);
    double expected = 1.0 / (2.0 * M_PI * R * C);
    REQUIRE_THAT(rc.cutoffHz(), WithinAbs(expected, 0.1));
}

TEST_CASE("RC_Element: LPF passes DC", "[Parts][RC]") {
    RC_Element rc(1000.0, 100e-9);
    rc.prepare(44100.0);
    // Feed constant 1.0 for many samples
    double out = 0.0;
    for (int i = 0; i < 10000; ++i) {
        out = rc.processLPF(1.0);
    }
    REQUIRE_THAT(out, WithinAbs(1.0, 0.01));
}

TEST_CASE("RC_Element: HPF blocks DC", "[Parts][RC]") {
    RC_Element rc(1000.0, 100e-9);
    rc.prepare(44100.0);
    double out = 0.0;
    for (int i = 0; i < 10000; ++i) {
        out = rc.processHPF(1.0);
    }
    REQUIRE_THAT(out, WithinAbs(0.0, 0.01));
}

TEST_CASE("RC_Element: AllPass preserves amplitude at DC", "[Parts][RC]") {
    RC_Element rc(1000.0, 100e-9);
    rc.prepare(44100.0);
    double out = 0.0;
    for (int i = 0; i < 10000; ++i) {
        out = rc.processAP(1.0);
    }
    // AllPass should converge to +/-1 for DC input
    REQUIRE(std::fabs(out) > 0.5);
}

TEST_CASE("RC_Element: setRC updates cutoff", "[Parts][RC]") {
    RC_Element rc(1000.0, 100e-9);
    double fc1 = rc.cutoffHz();
    rc.setRC(2000.0, 100e-9);
    double fc2 = rc.cutoffHz();
    REQUIRE(fc1 > fc2);
}

TEST_CASE("RC_Element: reset clears state", "[Parts][RC]") {
    RC_Element rc(1000.0, 100e-9);
    rc.prepare(44100.0);
    rc.processLPF(1.0);
    rc.processLPF(1.0);
    rc.reset();
    double out = rc.processLPF(0.0);
    REQUIRE_THAT(out, WithinAbs(0.0, 1e-15));
}
