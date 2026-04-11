// ============================================================================
// AdapterSag unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/power/AdapterSag.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("AdaSag: prepare and reset without crash", "[AdapterSag]") {
    AdapterSag as;
    as.prepare(2, kSR);
    as.reset();
    REQUIRE(true);
}

TEST_CASE("AdaSag: prepare with ProcessSpec", "[AdapterSag]") {
    AdapterSag as;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    as.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("AdaSag: Linear9V at light load -> near 9V", "[AdapterSag]") {
    AdapterSag as;
    as.prepare(1, kSR);
    AdapterSag::Params params;
    params.adapterType = AdapterSag::Linear9V;
    params.loadCurrentMa = 10.0f;
    params.sagAmount = 0.0f;
    params.rippleMix = 0.0f;

    float v = as.process(0, 0.0f, params);
    REQUIRE(v > 8.9f);
    REQUIRE(v <= 9.1f);
}

TEST_CASE("AdaSag: Linear18V at light load -> near 18V", "[AdapterSag]") {
    AdapterSag as;
    as.prepare(1, kSR);
    AdapterSag::Params params;
    params.adapterType = AdapterSag::Linear18V;
    params.loadCurrentMa = 10.0f;
    params.sagAmount = 0.0f;
    params.rippleMix = 0.0f;

    float v = as.process(0, 0.0f, params);
    REQUIRE(v > 17.9f);
    REQUIRE(v <= 18.1f);
}

TEST_CASE("AdaSag: Switching18V at light load -> near 18V", "[AdapterSag]") {
    AdapterSag as;
    as.prepare(1, kSR);
    AdapterSag::Params params;
    params.adapterType = AdapterSag::Switching18V;
    params.loadCurrentMa = 20.0f;
    params.sagAmount = 0.0f;
    params.rippleMix = 0.0f;

    float v = as.process(0, 0.0f, params);
    REQUIRE(v > 17.9f);
    REQUIRE(v <= 18.1f);
}

TEST_CASE("AdaSag: Unregulated9V drops more under load", "[AdapterSag]") {
    AdapterSag as;
    as.prepare(1, kSR);
    AdapterSag::Params params;
    params.adapterType = AdapterSag::Unregulated9V;
    params.sagAmount = 0.0f;
    params.rippleMix = 0.0f;

    params.loadCurrentMa = 10.0f;
    float vLight = as.process(0, 0.0f, params);

    params.loadCurrentMa = 100.0f;
    float vHeavy = as.process(0, 0.0f, params);

    REQUIRE(vLight > vHeavy);
    // 
    REQUIRE((vLight - vHeavy) > 0.3f);
}

TEST_CASE("AdaSag: Regulated drops less than unregulated", "[AdapterSag]") {
    float vReg = (float)AdapterSag::getStaticVoltage(AdapterSag::Linear9V, 100.0f);
    float vUnreg = (float)AdapterSag::getStaticVoltage(AdapterSag::Unregulated9V, 100.0f);

    // Same voltage class, regulated should be closer to nominal
    double nomReg = AdapterSag::getNominalVoltage(AdapterSag::Linear9V);
    double nomUnreg = AdapterSag::getNominalVoltage(AdapterSag::Unregulated9V);

    double dropReg = nomReg - vReg;
    double dropUnreg = nomUnreg - vUnreg;

    REQUIRE(dropReg < dropUnreg);
}

TEST_CASE("AdaSag: dynamic sag under loud signal", "[AdapterSag]") {
    AdapterSag as;
    as.prepare(1, kSR);
    AdapterSag::Params params;
    params.adapterType = AdapterSag::Unregulated9V;
    params.loadCurrentMa = 30.0f;
    params.sagAmount = 1.0f;
    params.attackMs = 1.0f;
    params.rippleMix = 0.0f;

    float vSilent = as.process(0, 0.0f, params);

    float vLoud = vSilent;
    for (int i = 0; i < 4410; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 200.0 * i / kSR);
        vLoud = as.process(0, in, params);
    }
    REQUIRE(vLoud < vSilent);
}

TEST_CASE("AdaSag: sagAmount=0 -> no dynamic sag", "[AdapterSag]") {
    AdapterSag as;
    as.prepare(1, kSR);
    AdapterSag::Params params;
    params.adapterType = AdapterSag::Unregulated9V;
    params.loadCurrentMa = 20.0f;
    params.sagAmount = 0.0f;
    params.rippleMix = 0.0f;

    float vSilent = as.process(0, 0.0f, params);

    float vLoud = vSilent;
    for (int i = 0; i < 4410; ++i) {
        float in = 0.9f * std::sin(2.0 * M_PI * 100.0 * i / kSR);
        vLoud = as.process(0, in, params);
    }
    REQUIRE(vLoud == Approx(vSilent).margin(0.01f));
}

TEST_CASE("AdaSag: processBlock passes audio through", "[AdapterSag]") {
    AdapterSag as;
    as.prepare(1, kSR);
    AdapterSag::Params params;

    constexpr int N = 256;
    std::vector<float> inBuf(N), outBuf(N), vBuf(N);
    for (int i = 0; i < N; ++i)
        inBuf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    const float* inPtr = inBuf.data();
    float* outPtr = outBuf.data();

    as.processBlock(&inPtr, &outPtr, vBuf.data(), 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(outBuf[i] == Approx(inBuf[i]));

    for (int i = 0; i < N; ++i)
        REQUIRE(vBuf[i] > 0.0f);
}

TEST_CASE("AdaSag: getNominalVoltage", "[AdapterSag]") {
    REQUIRE(AdapterSag::getNominalVoltage(AdapterSag::Linear9V) == Approx(9.0));
    REQUIRE(AdapterSag::getNominalVoltage(AdapterSag::Linear18V) == Approx(18.0));
    REQUIRE(AdapterSag::getNominalVoltage(AdapterSag::Switching9V) == Approx(9.0));
    REQUIRE(AdapterSag::getNominalVoltage(AdapterSag::Switching18V) == Approx(18.0));
    REQUIRE(AdapterSag::getNominalVoltage(AdapterSag::Unregulated9V) == Approx(9.6));
    REQUIRE(AdapterSag::getNominalVoltage(AdapterSag::Unregulated18V) == Approx(19.2));
}

TEST_CASE("AdaSag: voltageToSagLevel mapping", "[AdapterSag]") {
    // 9V adapter: 9V -> 1.0, 0V -> 0.0
    REQUIRE(AdapterSag::voltageToSagLevel(9.0f, AdapterSag::Linear9V) == Approx(1.0f));
    REQUIRE(AdapterSag::voltageToSagLevel(0.0f, AdapterSag::Linear9V) == Approx(0.0f));
    REQUIRE(AdapterSag::voltageToSagLevel(4.5f, AdapterSag::Linear9V) == Approx(0.5f));

    // 18V adapter
    REQUIRE(AdapterSag::voltageToSagLevel(18.0f, AdapterSag::Linear18V) == Approx(1.0f));
    REQUIRE(AdapterSag::voltageToSagLevel(9.0f, AdapterSag::Linear18V) == Approx(0.5f));
}

TEST_CASE("AdaSag: getAdapterSpec accessor", "[AdapterSag]") {
    const auto& spec = AdapterSag::getAdapterSpec(AdapterSag::Switching18V);
    REQUIRE(spec.nominalVoltage == Approx(18.0));
    REQUIRE(spec.isRegulated == true);

    const auto& unreg = AdapterSag::getAdapterSpec(AdapterSag::Unregulated9V);
    REQUIRE(unreg.isRegulated == false);
    REQUIRE(unreg.outputResistance > 1.0);

    // 
    const auto& clamped = AdapterSag::getAdapterSpec(99);
    REQUIRE(clamped.nominalVoltage > 0.0);
}

TEST_CASE("AdaSag: all adapter types produce valid voltage", "[AdapterSag]") {
    AdapterSag as;
    as.prepare(1, kSR);

    for (int type = 0; type < AdapterSag::kAdapterTypeCount; ++type) {
        AdapterSag::Params params;
        params.adapterType = type;
        params.loadCurrentMa = 30.0f;
        params.sagAmount = 1.0f;
        params.rippleMix = 1.0f;

        for (int i = 0; i < 1000; ++i) {
            float in = 0.5f * std::sin(2.0 * M_PI * 200.0 * i / kSR);
            float v = as.process(0, in, params);
            REQUIRE(v >= 0.0f);
            REQUIRE(v <= 25.0f);
        }
        as.reset();
    }
}

TEST_CASE("AdaSag: current limit causes voltage foldback", "[AdapterSag]") {
    // Unregulated9V: maxCurrent=300mA, knee=0.70 -> limit starts at 210mA
    float vNormal = (float)AdapterSag::getStaticVoltage(AdapterSag::Unregulated9V, 100.0f);
    float vOverload = (float)AdapterSag::getStaticVoltage(AdapterSag::Unregulated9V, 350.0f);

    REQUIRE(vOverload < vNormal);
    // Extreme overload should drop significantly
    REQUIRE(vOverload < 5.0f);
}

TEST_CASE("AdaSag: 18V types have higher voltage than 9V", "[AdapterSag]") {
    float v9 = (float)AdapterSag::getStaticVoltage(AdapterSag::Linear9V, 20.0f);
    float v18 = (float)AdapterSag::getStaticVoltage(AdapterSag::Linear18V, 20.0f);
    REQUIRE(v18 > v9);
    REQUIRE(v18 > 17.0f);

    float v9s = (float)AdapterSag::getStaticVoltage(AdapterSag::Switching9V, 20.0f);
    float v18s = (float)AdapterSag::getStaticVoltage(AdapterSag::Switching18V, 20.0f);
    REQUIRE(v18s > v9s);
}

TEST_CASE("AdaSag: output never negative", "[AdapterSag]") {
    AdapterSag as;
    as.prepare(1, kSR);
    AdapterSag::Params params;
    params.adapterType = AdapterSag::Unregulated9V;
    params.loadCurrentMa = 200.0f;
    params.sagAmount = 1.0f;
    params.attackMs = 0.5f;

    for (int i = 0; i < 44100; ++i) {
        float in = 5.0f * std::sin(2.0 * M_PI * 80.0 * i / kSR);
        float v = as.process(0, in, params);
        REQUIRE(v >= 0.0f);
    }
}
