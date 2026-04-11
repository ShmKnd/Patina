#pragma once
// BbdConstants.h — BBD (Bucket Brigade Device) chip constants
namespace PartsConstants {
    inline constexpr int    bbdStagesDefault = 8192;
    inline constexpr double bbdStagesReference = 1024.0;
    inline constexpr double bbdSupplyBwFactor = 0.15;
    inline constexpr double bbdSatHeadroomRatio = 0.9;
    inline constexpr double bbdSatSoftness = 1.2;
    inline constexpr double bbdCutoffRatio = 0.07;
    inline constexpr double bbdClockHzMin = 10000.0;
    inline constexpr double bbdClockHzMax = 100000.0;
    inline constexpr double bbdNoiseInjectionGain = 4.0;
    inline constexpr double bbdBaseNoise = 1e-6;
    inline constexpr double bbdNoiseHpFc = 6000.0;
    inline constexpr double bbdHfCompFc = 1500.0;
    inline constexpr bool   emulateBBD = true;
    inline constexpr double V_supplyMinEnable = 6.0;
}
