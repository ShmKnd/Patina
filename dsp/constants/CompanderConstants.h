#pragma once
// CompanderConstants.h — NE570 / SA571 compander IC constants
namespace PartsConstants {
    inline constexpr double C_T_NE570 = 0.68e-6;
    inline constexpr double R_T_NE570 = 120000.0;
    inline constexpr double C_det_NE570 = 1.0e-6;
    inline constexpr double R_det_NE570 = 10000.0;
    inline constexpr double R_pre_NE570 = 6400.0;
    inline constexpr double C_pre_NE570 = 10.0e-9;
    inline constexpr double V_ref_NE570 = 0.316;
    inline constexpr double I_charge_NE570 = 10.0e-6;
    inline constexpr double ne570VcaOutputRatio = 0.6;
    inline constexpr double ne570AttackSec = C_T_NE570 * V_ref_NE570 / (2.0 * I_charge_NE570);
    inline constexpr double ne570ReleaseSec = R_T_NE570 * C_T_NE570;
    inline constexpr double ne570RmsTimeSec = R_det_NE570 * C_det_NE570;
    inline constexpr double ne570EmphasisFcHz = 1.0 / (2.0 * 3.14159265358979323846 * R_pre_NE570 * C_pre_NE570);
    inline constexpr double compPreEmphasisGain = 1.5;
    inline constexpr double compDeEmphasisGain = 2.5;
}
