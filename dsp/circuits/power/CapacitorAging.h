#pragma once
#include <cmath>
#include <algorithm>
#include "../../constants/PartsConstants.h"
#include "../../config/ModdingConfig.h"

// Capacitor aging simulation
// - Capacitance decrease & ESR increase over time
// - Temperature-accelerated degradation (Arrhenius law based)
// - ModdingConfig::CapGradeSpec dynamic modulation of
// - Output to BbdDelayEngine capacitanceScale
//
// Degradation model:
//   C(t) = C_0 × (1 - k × age)   Capacitance decreases proportionally with age
//   ESR(t) = ESR_0 × (1 + m × age²)  ESR increases with the square of age (electrolyte evaporation)
//   Temperature acceleration: age_eff = age × 2^((T - 25) / 10)  degradation rate doubles per 10°C rise
//
// usage:
//   CapacitorAging aging;
//   aging.setAge(yearsOld);
//   aging.setTemperature(35.0f);
//   double capScale = aging.getCapacitanceScale(baseGrade);
//   double esrScale = aging.getEsrScale(baseGrade);
class CapacitorAging
{
public:
    struct Params
    {
        float ageYears       = 0.0f;    // Age in years (0=new, 40=vintage)
        float temperature    = 25.0f;   // Ambient temperature (°C)  25°C=reference
        float qualityFactor  = 1.0f;    // Quality factor (0.5=cheap, 1.0=standard, 2.0=high quality)
                                         // Higher quality degrades more slowly
        float humidityFactor = 1.0f;    // Humidity acceleration (1.0=standard, 1.5=high humidity)
    };

    CapacitorAging() noexcept = default;

    void setParams(const Params& p) noexcept { params = p; }

    void setAge(float years) noexcept { params.ageYears = std::max(0.0f, years); }
    void setTemperature(float tempC) noexcept { params.temperature = tempC; }
    void setQualityFactor(float q) noexcept { params.qualityFactor = std::max(0.1f, q); }
    void setHumidityFactor(float h) noexcept { params.humidityFactor = std::max(0.5f, h); }

    // ====== Get output values ======

    // capacitanceScale: Remaining capacitance ratio (1.0=new, 0.5=minimum)
    // Can be connected to BbdDelayEngine params.capacitanceScale
    double getCapacitanceScale() const noexcept
    {
        const double effAge = effectiveAge();
        // Capacitance decrease: k% per year (per AgingConstants)
        const double k = PartsConstants::aging_k_cap_perYear / std::max(0.1, (double)params.qualityFactor);
        const double scale = 1.0 - k * effAge;
        return std::clamp(scale, PartsConstants::aging_min_cap_scale, 1.0);
    }

    // capacitanceScale considering CapGrade
    double getCapacitanceScale(const ModdingConfig::CapGradeSpec& grade) const noexcept
    {
        const double baseScale = getCapacitanceScale();
        // CapGrade toleranceScale represents manufacturing variation — variation widens further during degradation
        // AudioGrade (tolerance=0.15) is resistant to degradation (high quality)
        const double gradeProtection = 1.0 - 0.3 * (1.0 - grade.toleranceScale);
        // gradeProtection: Standard=0.7, Film=0.805, AudioGrade=0.845
        const double adjusted = 1.0 - (1.0 - baseScale) * gradeProtection;
        return std::clamp(adjusted, PartsConstants::aging_min_cap_scale, 1.0);
    }

    // ESR scale: Rate of increase in equivalent series resistance (1.0=new, higher=degraded)
    double getEsrScale() const noexcept
    {
        const double effAge = effectiveAge();
        // ESR increases proportionally to the square of age (electrolyte evaporation model)
        const double m = 0.005 / std::max(0.1, (double)params.qualityFactor);
        const double esrIncrease = m * effAge * effAge;
        return std::clamp(1.0 + esrIncrease, 1.0, 5.0);  // up to 5x maximum
    }

    // ESR scale considering CapGrade
    double getEsrScale(const ModdingConfig::CapGradeSpec& grade) const noexcept
    {
        const double baseEsr = getEsrScale();
        // Multiply CapGrade base ESR by degradation factor
        return grade.esr * baseEsr;
    }

    // Microphonics noise scale: increases with degradation
    double getMicrophonicsScale() const noexcept
    {
        const double effAge = effectiveAge();
        // Older capacitors have more microphonics
        const double increase = 0.02 * effAge / std::max(0.1, (double)params.qualityFactor);
        return std::clamp(1.0 + increase, 1.0, 3.0);
    }

    // Microphonics considering CapGrade
    double getMicrophonicsScale(const ModdingConfig::CapGradeSpec& grade) const noexcept
    {
        return grade.microphonicsScale * getMicrophonicsScale();
    }

    // Generate post-degradation CapGradeSpec (in format passable to ModdingConfig)
    ModdingConfig::CapGradeSpec getAgedSpec(const ModdingConfig::CapGradeSpec& baseGrade) const noexcept
    {
        ModdingConfig::CapGradeSpec aged;
        aged.name = baseGrade.name;
        // toleranceScale: Manufacturing tolerance increases with degradation
        const double effAge = effectiveAge();
        const double tolIncrease = 0.01 * effAge / std::max(0.1, (double)params.qualityFactor);
        aged.toleranceScale = std::clamp(baseGrade.toleranceScale + tolIncrease, 0.0, 2.0);
        aged.esr = getEsrScale(baseGrade);
        aged.microphonicsScale = getMicrophonicsScale(baseGrade);
        return aged;
    }

    // Get effective age (for UI display)
    double getEffectiveAge() const noexcept { return effectiveAge(); }

    // For debugging: full parameter summary
    struct AgingSummary
    {
        double effectiveAge;
        double capacitanceScale;
        double esrScale;
        double microphonicsScale;
    };

    AgingSummary getSummary() const noexcept
    {
        return { effectiveAge(), getCapacitanceScale(), getEsrScale(), getMicrophonicsScale() };
    }

    AgingSummary getSummary(const ModdingConfig::CapGradeSpec& grade) const noexcept
    {
        return { effectiveAge(),
                 getCapacitanceScale(grade),
                 getEsrScale(grade),
                 getMicrophonicsScale(grade) };
    }

private:
    // Effective age considering temperature and humidity (Arrhenius law based)
    // Degradation rate doubles per 10°C rise
    double effectiveAge() const noexcept
    {
        const double age = std::max(0.0, (double)params.ageYears);
        // Temperature acceleration: 2^((T - 25) / 10)
        const double tempAccel = std::pow(2.0, ((double)params.temperature - 25.0) / 10.0);
        // Humidity acceleration
        const double humAccel = std::max(0.5, (double)params.humidityFactor);
        return age * std::max(0.1, tempAccel) * humAccel;
    }

    Params params;
};
