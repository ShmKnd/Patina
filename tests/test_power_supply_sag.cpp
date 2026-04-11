// ============================================================================
// PowerSupplySag unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/power/PowerSupplySag.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("PSS: prepare and reset without crash", "[PowerSupplySag]") {
    PowerSupplySag pss;
    pss.prepare(2, kSR);
    pss.reset();
    REQUIRE(true);
}

TEST_CASE("PSS: prepare with ProcessSpec", "[PowerSupplySag]") {
    PowerSupplySag pss;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    pss.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("PSS: silence -> no sag (output near 1.0)", "[PowerSupplySag]") {
    PowerSupplySag pss;
    pss.prepare(1, kSR);
    PowerSupplySag::Params params;

    // Process silence for a while
    float sag = 0.0f;
    for (int i = 0; i < 4410; ++i) {
        sag = pss.process(0, 0.0f, params);
    }
    // No sag during silence (near 1.0)
    REQUIRE(sag > 0.95f);
    REQUIRE(sag <= 1.0f);
}

TEST_CASE("PSS: loud signal -> voltage drops", "[PowerSupplySag]") {
    PowerSupplySag pss;
    pss.prepare(1, kSR);
    PowerSupplySag::Params params;
    params.sagDepth = 1.0f;
    params.attackMs = 1.0f;

    // Apply a large signal
    float sag = 1.0f;
    for (int i = 0; i < 4410; ++i) {
        float in = 0.9f * std::sin(2.0 * M_PI * 100.0 * i / kSR);
        sag = pss.process(0, in, params);
    }
    // Sag occurs (less than 1.0)
    REQUIRE(sag < 1.0f);
    REQUIRE(sag > 0.0f);
}

TEST_CASE("PSS: voltage recovers after signal stops", "[PowerSupplySag]") {
    PowerSupplySag pss;
    pss.prepare(1, kSR);
    PowerSupplySag::Params params;
    params.attackMs = 1.0f;
    params.releaseMs = 20.0f;
    params.sagDepth = 1.0f;

    // 
    for (int i = 0; i < 4410; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 200.0 * i / kSR);
        pss.process(0, in, params);
    }

    float sagDuring = pss.getSagLevel(0);

    // Recover during silence
    for (int i = 0; i < 44100; ++i) {
        pss.process(0, 0.0f, params);
    }

    float sagAfter = pss.getSagLevel(0);
    // Sag reduced after recovery
    REQUIRE(sagAfter > sagDuring);
}

TEST_CASE("PSS: higher rectifier resistance -> deeper sag", "[PowerSupplySag]") {
    PowerSupplySag pss1, pss2;
    pss1.prepare(1, kSR);
    pss2.prepare(1, kSR);

    PowerSupplySag::Params p1, p2;
    p1.rectifierResistance = 25.0f;   // GZ34
    p2.rectifierResistance = 60.0f;   // 5Y3 (higher resistance)
    p1.sagDepth = p2.sagDepth = 1.0f;
    p1.attackMs = p2.attackMs = 1.0f;

    float sag1 = 1.0f, sag2 = 1.0f;
    for (int i = 0; i < 8820; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 100.0 * i / kSR);
        sag1 = pss1.process(0, in, p1);
        sag2 = pss2.process(0, in, p2);
    }

    // Higher-resistance rectifier shows deeper sag
    REQUIRE(sag2 < sag1);
}

TEST_CASE("PSS: sagDepth=0 -> no sag", "[PowerSupplySag]") {
    PowerSupplySag pss;
    pss.prepare(1, kSR);
    PowerSupplySag::Params params;
    params.sagDepth = 0.0f;

    float sag = 1.0f;
    for (int i = 0; i < 4410; ++i) {
        float in = 0.9f * std::sin(2.0 * M_PI * 100.0 * i / kSR);
        sag = pss.process(0, in, params);
    }
    // No sag when sagDepth=0
    REQUIRE(sag == Approx(1.0f).margin(0.01f));
}

TEST_CASE("PSS: processBlock passes audio through", "[PowerSupplySag]") {
    PowerSupplySag pss;
    pss.prepare(1, kSR);
    PowerSupplySag::Params params;

    constexpr int N = 256;
    std::vector<float> inBuf(N), outBuf(N), sagBuf(N);
    for (int i = 0; i < N; ++i)
        inBuf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    const float* inPtr = inBuf.data();
    float* outPtr = outBuf.data();

    pss.processBlock(&inPtr, &outPtr, sagBuf.data(), 1, N, params);

    // Audio is passing through
    for (int i = 0; i < N; ++i)
        REQUIRE(outBuf[i] == Approx(inBuf[i]));

    // Values are written into sagOut
    for (int i = 0; i < N; ++i) {
        REQUIRE(sagBuf[i] >= 0.0f);
        REQUIRE(sagBuf[i] <= 1.0f);
    }
}

TEST_CASE("PSS: sagToSupplyVoltage mapping", "[PowerSupplySag]") {
    // sag=1.0 -> V_supplyMax
    REQUIRE(PowerSupplySag::sagToSupplyVoltage(1.0f) == Approx(PartsConstants::V_supplyMax));
    // sag=0.0 -> V_supplyMin
    REQUIRE(PowerSupplySag::sagToSupplyVoltage(0.0f) == Approx(PartsConstants::V_supplyMin));
    // sag=0.5 -> intermediate
    double mid = (PartsConstants::V_supplyMin + PartsConstants::V_supplyMax) / 2.0;
    REQUIRE(PowerSupplySag::sagToSupplyVoltage(0.5f) == Approx(mid));
}

TEST_CASE("PSS: sagToBPlusVoltage mapping", "[PowerSupplySag]") {
    REQUIRE(PowerSupplySag::sagToBPlusVoltage(1.0f) == Approx(450.0));
    REQUIRE(PowerSupplySag::sagToBPlusVoltage(0.5f) == Approx(225.0));
    REQUIRE(PowerSupplySag::sagToBPlusVoltage(0.0f) == Approx(0.0));
    // custom B+
    REQUIRE(PowerSupplySag::sagToBPlusVoltage(1.0f, 350.0) == Approx(350.0));
}

TEST_CASE("PSS: getSagLevel returns valid range", "[PowerSupplySag]") {
    PowerSupplySag pss;
    pss.prepare(2, kSR);
    PowerSupplySag::Params params;

    for (int i = 0; i < 1000; ++i) {
        pss.process(0, 0.5f * std::sin(2.0 * M_PI * 100.0 * i / kSR), params);
    }

    float level = pss.getSagLevel(0);
    REQUIRE(level >= 0.0f);
    REQUIRE(level <= 1.0f);

    // invalid channel
    REQUIRE(pss.getSagLevel(-1) == 1.0f);
    REQUIRE(pss.getSagLevel(99) == 1.0f);
}

TEST_CASE("PSS: temperature affects sag", "[PowerSupplySag]") {
    PowerSupplySag pss_cold, pss_hot;
    pss_cold.prepare(1, kSR);
    pss_hot.prepare(1, kSR);

    PowerSupplySag::Params pCold, pHot;
    pCold.temperature = 10.0f;
    pHot.temperature = 50.0f;
    pCold.sagDepth = pHot.sagDepth = 1.0f;
    pCold.attackMs = pHot.attackMs = 1.0f;

    float sagCold = 1.0f, sagHot = 1.0f;
    for (int i = 0; i < 8820; ++i) {
        float in = 0.7f * std::sin(2.0 * M_PI * 100.0 * i / kSR);
        sagCold = pss_cold.process(0, in, pCold);
        sagHot = pss_hot.process(0, in, pHot);
    }
    // At high temperature rectifier resistance increases -> deeper sag
    REQUIRE(sagHot < sagCold);
}

TEST_CASE("PSS: output always in valid range under extreme input", "[PowerSupplySag]") {
    PowerSupplySag pss;
    pss.prepare(1, kSR);
    PowerSupplySag::Params params;
    params.sagDepth = 1.0f;
    params.rectifierResistance = 100.0f;

    for (int i = 0; i < 44100; ++i) {
        float in = 10.0f * std::sin(2.0 * M_PI * 50.0 * i / kSR);  // 
        float sag = pss.process(0, in, params);
        REQUIRE(sag >= 0.0f);
        REQUIRE(sag <= 1.0f);
    }
}
