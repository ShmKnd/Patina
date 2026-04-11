#pragma once

#include "../constants/PartsConstants.h"
#include "ModdingConfig.h"
#include "../circuits/compander/CompanderModule.h"
#include "../circuits/bbd/BbdStageEmulator.h"

namespace patina {
namespace presets {

// Compander IC presets

inline CompanderModule::Config ne570()
{
    const auto& spec = ModdingConfig::companderSpecs[ModdingConfig::NE570];
    CompanderModule::Config cfg;
    cfg.vcaOutputRatio = spec.vcaOutputRatio;
    cfg.timing.compressorAttack  = CompanderParams::COMP_ATTACK_SEC * spec.attackScale;
    cfg.timing.compressorRelease = CompanderParams::COMP_RELEASE_SEC * spec.releaseScale;
    cfg.timing.expanderAttack    = CompanderParams::EXP_ATTACK_SEC * spec.attackScale;
    cfg.timing.expanderRelease   = CompanderParams::EXP_RELEASE_SEC * spec.releaseScale;
    return cfg;
}

inline CompanderModule::Config sa571n()
{
    const auto& spec = ModdingConfig::companderSpecs[ModdingConfig::SA571N];
    CompanderModule::Config cfg;
    cfg.vcaOutputRatio = spec.vcaOutputRatio;
    cfg.timing.compressorAttack  = CompanderParams::COMP_ATTACK_SEC * spec.attackScale;
    cfg.timing.compressorRelease = CompanderParams::COMP_RELEASE_SEC * spec.releaseScale;
    cfg.timing.expanderAttack    = CompanderParams::EXP_ATTACK_SEC * spec.attackScale;
    cfg.timing.expanderRelease   = CompanderParams::EXP_RELEASE_SEC * spec.releaseScale;
    return cfg;
}

// Op-amp presets
// Unified as OpAmpPrimitive::Spec (replaces old BbdFeedback::OpAmpOverrides)

inline ModdingConfig::OpAmpSpec tl072cp()  { return ModdingConfig::opAmpSpecs[ModdingConfig::TL072CP]; }
inline ModdingConfig::OpAmpSpec jrc4558d() { return ModdingConfig::opAmpSpecs[ModdingConfig::JRC4558D]; }
inline ModdingConfig::OpAmpSpec opa2134()  { return ModdingConfig::opAmpSpecs[ModdingConfig::OPA2134]; }
inline ModdingConfig::OpAmpSpec lm4562()   { return ModdingConfig::opAmpSpecs[ModdingConfig::LM4562]; }
inline ModdingConfig::OpAmpSpec ne5532()   { return ModdingConfig::opAmpSpecs[ModdingConfig::NE5532]; }
inline ModdingConfig::OpAmpSpec lm741()    { return ModdingConfig::opAmpSpecs[ModdingConfig::LM741]; }

// Get OpAmpSpec from OpAmpType enum
inline ModdingConfig::OpAmpSpec opAmpFromType(int opAmpType)
{
    return ModdingConfig::opAmpSpecs[std::clamp(opAmpType, 0, (int)ModdingConfig::kOpAmpCount - 1)];
}

// BBD chip presets

// MN3005: 4096 stages (standalone), high-quality BBD
inline BbdStageEmulator::Params mn3005(double supplyVoltage = 15.0)
{
    BbdStageEmulator::Params p;
    p.stages = 4096;
    p.supplyVoltage = supplyVoltage;
    return p;
}

// MN3207: 1024 stages, compact BBD
inline BbdStageEmulator::Params mn3207(double supplyVoltage = 9.0)
{
    BbdStageEmulator::Params p;
    p.stages = 1024;
    p.supplyVoltage = supplyVoltage;
    return p;
}

// MN3005 x 2 in series: 8192 stages (classic BBD delay unit configuration)
inline BbdStageEmulator::Params mn3005Dual(double supplyVoltage = 15.0)
{
    BbdStageEmulator::Params p;
    p.stages = PartsConstants::bbdStagesDefault; // 8192
    p.supplyVoltage = supplyVoltage;
    return p;
}

// CapGrade bandwidth scale

// Calculate values for BbdStageEmulator::setBandwidthScale from ModdingConfig::CapGrade enum
inline double capGradeBandwidthScale(int capGrade)
{
    const auto& spec = ModdingConfig::capGradeSpecs[std::clamp(capGrade, 0, (int)ModdingConfig::kCapGradeCount - 1)];
    return std::sqrt(1.0 / std::max(0.01, spec.esr));
}

} // namespace presets
} // namespace patina
