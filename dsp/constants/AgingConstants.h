#pragma once
// AgingConstants.h — Aging simulation constants
namespace PartsConstants {
    inline constexpr double aging_k_cap_perYear = 0.01;
    inline constexpr double aging_da_perYear = 0.02;
    inline constexpr double aging_da_max = 0.6;
    inline constexpr double aging_opAmpNoise_perYear = 0.01;
    inline constexpr double aging_clockJitter_perYear = 0.0005;
    inline constexpr double aging_min_cap_scale = 0.5;
    inline constexpr double aging_manufacture_variance_default = 0.5;

    // DA degradation parameters
    inline constexpr double aging_da_supplyReduction = 0.25;
    inline constexpr double aging_da_alpha = 0.005;
}
