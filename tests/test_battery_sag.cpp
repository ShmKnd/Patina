// ============================================================================
// BatterySag unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/power/BatterySag.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("BatSag: prepare and reset without crash", "[BatterySag]") {
    BatterySag bs;
    bs.prepare(2, kSR);
    bs.reset();
    REQUIRE(true);
}

TEST_CASE("BatSag: prepare with ProcessSpec", "[BatterySag]") {
    BatterySag bs;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    bs.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("BatSag: fresh battery -> near nominal voltage", "[BatterySag]") {
    BatterySag bs;
    bs.prepare(1, kSR);
    BatterySag::Params params;
    params.batteryType = BatterySag::Alkaline;
    params.batteryLife = 1.0f;
    params.loadCurrentMa = 10.0f;
    params.sagAmount = 0.0f;  // No dynamic sag

    // At silence: open-circuit voltage minus idle draw
    float v = bs.process(0, 0.0f, params);
    // Alkaline fresh: 9.6V - 0.01A x 3Ohm = 9.57V
    REQUIRE(v > 9.0f);
    REQUIRE(v <= 10.0f);
}

TEST_CASE("BatSag: dead battery -> low voltage", "[BatterySag]") {
    BatterySag bs;
    bs.prepare(1, kSR);
    BatterySag::Params params;
    params.batteryType = BatterySag::Alkaline;
    params.batteryLife = 0.05f;  // Nearly empty
    params.loadCurrentMa = 15.0f;
    params.sagAmount = 0.0f;

    float v = bs.process(0, 0.0f, params);
    // Worn batteries have lower voltage
    REQUIRE(v < 7.0f);
    REQUIRE(v > 0.0f);
}

TEST_CASE("BatSag: lower life -> lower voltage", "[BatterySag]") {
    BatterySag bs;
    bs.prepare(1, kSR);
    BatterySag::Params params;
    params.batteryType = BatterySag::CarbonZinc;
    params.loadCurrentMa = 10.0f;
    params.sagAmount = 0.0f;

    params.batteryLife = 1.0f;
    float vFresh = bs.process(0, 0.0f, params);

    params.batteryLife = 0.3f;
    float vUsed = bs.process(0, 0.0f, params);

    REQUIRE(vFresh > vUsed);
}

TEST_CASE("BatSag: CarbonZinc has higher internal resistance", "[BatterySag]") {
    // For same charge/current, CarbonZinc shows lower voltage (higher internal resistance)
    BatterySag bs1, bs2;
    bs1.prepare(1, kSR);
    bs2.prepare(1, kSR);

    BatterySag::Params p1, p2;
    p1.batteryType = BatterySag::Alkaline;
    p2.batteryType = BatterySag::CarbonZinc;
    p1.batteryLife = p2.batteryLife = 0.5f;
    p1.loadCurrentMa = p2.loadCurrentMa = 20.0f;
    p1.sagAmount = p2.sagAmount = 0.0f;

    float v1 = bs1.process(0, 0.0f, p1);
    float v2 = bs2.process(0, 0.0f, p2);

    // Alkaline (R=6Ohm) vs CarbonZinc (R=20Ohm) at 0.5 life -> CarbonZinc drops more
    REQUIRE(v1 > v2);
}

TEST_CASE("BatSag: dynamic sag under loud signal", "[BatterySag]") {
    BatterySag bs;
    bs.prepare(1, kSR);
    BatterySag::Params params;
    params.batteryType = BatterySag::CarbonZinc;
    params.batteryLife = 0.5f;
    params.loadCurrentMa = 15.0f;
    params.sagAmount = 1.0f;
    params.attackMs = 1.0f;

    // First record voltage with no signal
    float vSilent = bs.process(0, 0.0f, params);

    // Dynamic sag under large signal
    float vLoud = vSilent;
    for (int i = 0; i < 4410; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 200.0 * i / kSR);
        vLoud = bs.process(0, in, params);
    }

    // Voltage drops further when signal is present
    REQUIRE(vLoud < vSilent);
}

TEST_CASE("BatSag: sagAmount=0 -> no dynamic sag", "[BatterySag]") {
    BatterySag bs;
    bs.prepare(1, kSR);
    BatterySag::Params params;
    params.batteryType = BatterySag::Alkaline;
    params.batteryLife = 0.5f;
    params.loadCurrentMa = 10.0f;
    params.sagAmount = 0.0f;

    float vSilent = bs.process(0, 0.0f, params);

    // Voltage unchanged even with large input signal
    float vLoud = vSilent;
    for (int i = 0; i < 4410; ++i) {
        float in = 0.9f * std::sin(2.0 * M_PI * 100.0 * i / kSR);
        vLoud = bs.process(0, in, params);
    }

    REQUIRE(vLoud == Approx(vSilent).margin(0.01f));
}

TEST_CASE("BatSag: processBlock passes audio through", "[BatterySag]") {
    BatterySag bs;
    bs.prepare(1, kSR);
    BatterySag::Params params;

    constexpr int N = 256;
    std::vector<float> inBuf(N), outBuf(N), vBuf(N);
    for (int i = 0; i < N; ++i)
        inBuf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    const float* inPtr = inBuf.data();
    float* outPtr = outBuf.data();

    bs.processBlock(&inPtr, &outPtr, vBuf.data(), 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(outBuf[i] == Approx(inBuf[i]));

    for (int i = 0; i < N; ++i) {
        REQUIRE(vBuf[i] > 0.0f);
        REQUIRE(vBuf[i] <= 10.0f);
    }
}

TEST_CASE("BatSag: getOpenCircuitVoltage boundaries", "[BatterySag]") {
    // life=1.0 -> fresh voltage
    double vFresh = BatterySag::getOpenCircuitVoltage(BatterySag::Alkaline, 1.0f);
    REQUIRE(vFresh == Approx(9.6).margin(0.01));

    // life=0.0 -> dead voltage
    double vDead = BatterySag::getOpenCircuitVoltage(BatterySag::Alkaline, 0.0f);
    REQUIRE(vDead == Approx(5.5).margin(0.01));
}

TEST_CASE("BatSag: getInternalResistance increases with drain", "[BatterySag]") {
    double rFresh = BatterySag::getInternalResistance(BatterySag::Alkaline, 1.0f);
    double rHalf = BatterySag::getInternalResistance(BatterySag::Alkaline, 0.5f);
    double rLow = BatterySag::getInternalResistance(BatterySag::Alkaline, 0.1f);

    REQUIRE(rHalf > rFresh);
    REQUIRE(rLow > rHalf);
}

TEST_CASE("BatSag: voltageToSagLevel mapping", "[BatterySag]") {
    REQUIRE(BatterySag::voltageToSagLevel(10.0f) == Approx(1.0f));
    REQUIRE(BatterySag::voltageToSagLevel(5.0f) == Approx(0.0f));
    REQUIRE(BatterySag::voltageToSagLevel(7.5f) == Approx(0.5f));
}

TEST_CASE("BatSag: getVoltage helper", "[BatterySag]") {
    BatterySag bs;
    float v = bs.getVoltage(BatterySag::Alkaline, 1.0f, 10.0f);
    // 9.6V - 0.01A x 3Ohm = 9.57V
    REQUIRE(v == Approx(9.57f).margin(0.02f));
}

TEST_CASE("BatSag: Rechargeable flat discharge curve", "[BatterySag]") {
    // Rechargeable has curve=0.1 -> very flat then sharp drop
    double v90 = BatterySag::getOpenCircuitVoltage(BatterySag::Rechargeable, 0.9f);
    double v50 = BatterySag::getOpenCircuitVoltage(BatterySag::Rechargeable, 0.5f);
    double v10 = BatterySag::getOpenCircuitVoltage(BatterySag::Rechargeable, 0.1f);

    // 0.9  0.5 
    REQUIRE(std::abs(v90 - v50) < 0.5);
    // 0.1 
    REQUIRE(v50 - v10 > 0.3);
}

TEST_CASE("BatSag: getBatterySpec accessor", "[BatterySag]") {
    const auto& alk = BatterySag::getBatterySpec(BatterySag::Alkaline);
    REQUIRE(alk.freshVoltage == Approx(9.6));

    const auto& cz = BatterySag::getBatterySpec(BatterySag::CarbonZinc);
    REQUIRE(cz.freshResistance == Approx(10.0));

    // 
    const auto& clamped = BatterySag::getBatterySpec(99);
    REQUIRE(clamped.freshVoltage > 0.0);
}

TEST_CASE("BatSag: output never negative under extreme drain", "[BatterySag]") {
    BatterySag bs;
    bs.prepare(1, kSR);
    BatterySag::Params params;
    params.batteryType = BatterySag::CarbonZinc;
    params.batteryLife = 0.02f;
    params.loadCurrentMa = 50.0f;
    params.sagAmount = 1.0f;
    params.attackMs = 0.5f;

    for (int i = 0; i < 44100; ++i) {
        float in = 5.0f * std::sin(2.0 * M_PI * 80.0 * i / kSR);
        float v = bs.process(0, in, params);
        REQUIRE(v >= 0.0f);
    }
}
