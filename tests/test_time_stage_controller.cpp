// ============================================================================
// BbdTimeController unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/bbd/BbdTimeController.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("BbdTimeController: Simple mode resolves valid result", "[BbdTimeController]") {
    BbdTimeController ctrl;
    auto r = ctrl.resolve(100.0, 0, 0, kSampleRate, 0, true, 0.0);

    REQUIRE(r.stagesForProcessing > 0);
    REQUIRE(r.stagesForProcessing <= 8192);
    REQUIRE(r.actualDelaySamples >= 1.5);
    REQUIRE(r.desiredDelaySamples > 0.0);
}

TEST_CASE("BbdTimeController: Classic mode returns fixed stages", "[BbdTimeController]") {
    BbdTimeController ctrl;
    
    auto r0 = ctrl.resolve(100.0, 1, 0, kSampleRate, 0, true, 0.0, 0);
    REQUIRE(r0.stagesForProcessing == 1024);

    auto r1 = ctrl.resolve(100.0, 1, 0, kSampleRate, 0, true, 0.0, 1);
    REQUIRE(r1.stagesForProcessing == 2048);

    auto r2 = ctrl.resolve(100.0, 1, 0, kSampleRate, 0, true, 0.0, 2);
    REQUIRE(r2.stagesForProcessing == 4096);

    auto r3 = ctrl.resolve(100.0, 1, 0, kSampleRate, 0, true, 0.0, 3);
    REQUIRE(r3.stagesForProcessing == 8192);
}

TEST_CASE("BbdTimeController: Expert mode respects user stages", "[BbdTimeController]") {
    BbdTimeController ctrl;
    auto r = ctrl.resolve(100.0, 2, 2048, kSampleRate, 0, true, 0.0);
    REQUIRE(r.stagesForProcessing == 2048);
}

TEST_CASE("BbdTimeController: Expert mode clamps stages 1-8192", "[BbdTimeController]") {
    BbdTimeController ctrl;
    auto r0 = ctrl.resolve(100.0, 2, 0, kSampleRate, 0, true, 0.0);
    REQUIRE(r0.stagesForProcessing >= 1);

    auto r1 = ctrl.resolve(100.0, 2, 99999, kSampleRate, 0, true, 0.0);
    REQUIRE(r1.stagesForProcessing <= 8192);
}

TEST_CASE("BbdTimeController: minimum actual delay is 1.5 samples", "[BbdTimeController]") {
    BbdTimeController ctrl;
    auto r = ctrl.resolve(0.0, 0, 0, kSampleRate, 0, true, 0.0);
    REQUIRE(r.actualDelaySamples >= 1.5);
}

TEST_CASE("BbdTimeController: desired delay matches timexsampleRate", "[BbdTimeController]") {
    BbdTimeController ctrl;
    double timeMs = 200.0;
    auto r = ctrl.resolve(timeMs, 0, 0, kSampleRate, 0, true, 0.0);
    double expected = timeMs * 0.001 * kSampleRate;
    REQUIRE(r.desiredDelaySamples == Approx(expected).margin(0.01));
}

TEST_CASE("BbdTimeController: BBD clock computed when emulateBBD=true", "[BbdTimeController]") {
    BbdTimeController ctrl;
    auto r = ctrl.resolve(100.0, 1, 0, kSampleRate, 0, true, 0.0, 2);
    REQUIRE(r.bbdClockHz > 0.0);
}

TEST_CASE("BbdTimeController: BBD clock zero when emulateBBD=false", "[BbdTimeController]") {
    BbdTimeController ctrl;
    auto r = ctrl.resolve(100.0, 1, 0, kSampleRate, 0, false, 0.0, 2);
    REQUIRE(r.bbdClockHz == 0.0);
}

TEST_CASE("BbdTimeController: Simple mode smoothing", "[BbdTimeController]") {
    BbdTimeController ctrl;
    
    // First resolve with long delay
    auto r1 = ctrl.resolve(500.0, 0, 0, kSampleRate, 0, true, 0.0);
    int stages1 = r1.stagesForProcessing;

    // Now resolve with shorter delay - should smoothly decrease
    auto r2 = ctrl.resolve(10.0, 0, 0, kSampleRate, r1.lastSmoothedStagesNext, true, 0.0);
    int stages2 = r2.stagesForProcessing;

    // Due to smoothing (alpha=0.12), stages shouldn't jump instantly down
    REQUIRE(stages2 <= stages1);
    // But not all the way down to the target yet (smoothing)
    auto rDirect = ctrl.resolve(10.0, 0, 0, kSampleRate, 0, true, 0.0);
    // stages2 should be >= direct if smoothing is working
    REQUIRE(stages2 >= rDirect.stagesForProcessing);
}

TEST_CASE("BbdTimeController: Classic mode clamps selector index", "[BbdTimeController]") {
    BbdTimeController ctrl;
    // Out of range index should be clamped
    auto r = ctrl.resolve(100.0, 1, 0, kSampleRate, 0, true, 0.0, 10);
    REQUIRE(r.stagesForProcessing == 8192); // Clamped to index 3
}
