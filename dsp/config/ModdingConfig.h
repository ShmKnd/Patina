#pragma once
// ModdingConfig.h
// Data model for part-swap MOD simulation.
// Not registered in APVTS (no DAW automation). State persisted via XML.

#include <vector>
#include <cstring>
#include <string>
#include "../constants/PartsConstants.h"
#include "../core/AudioCompat.h"
#include "../parts/OpAmpPrimitive.h"

struct ModdingConfig
{
    // ==================== Op-Amp selection ====================
    enum OpAmpType : int
    {
        TL072CP  = 0,   // Default; balanced vintage, bright and open
        JRC4558D,       // Vintage warm; original stock part
        OPA2134,        // Hi-fi clean; extended headroom
        LM4562,         // Studio transparent; lowest noise
        NE5532,         // Punchy & dense; console warmth
        LM741,          // Vintage raw; 1968 OG op-amp
        kOpAmpCount
    };

    // ==================== Compander IC selection ====================
    enum CompanderType : int
    {
        NE570  = 0,     // Stock; relaxed timing
        SA571N,         // Tight; fast attack & release
        kCompanderCount
    };

    // ==================== Capacitor Grade ====================
    enum CapGrade : int
    {
        Standard = 0,   // Standard ceramic/electrolytic
        Film,           // Film capacitor upgrade
        AudioGrade,     // Hi-fi grade (e.g. Nichicon MUSE)
        kCapGradeCount
    };

    // ==================== Current selection ====================
    int opAmp     = TL072CP;  // Default: TL072CP (balanced vintage)
    int compander = NE570;
    int capGrade  = Standard;

    // ===========================================================
    //  Each part's 'datasheet value' — delegated to OpAmpPrimitive
    // ===========================================================

    using OpAmpSpec = OpAmpPrimitive::Spec;

    struct CompanderSpec
    {
        const char* name;
        double attackScale;       // Attack time multiplier (NE570=1.0 baseline)
        double releaseScale;      // Release time multiplier (NE570=1.0 baseline)
        double vcaOutputRatio;
        double noiseScale;        // Relative noise (NE570=1.0 baseline)
    };

    struct CapGradeSpec
    {
        const char* name;
        double toleranceScale;    // Capacitance tolerance (1.0=±20%, 0.4=±10%, 0.2=±5%)
        double esr;               // Equivalent series resistance (relative, Stock=1.0)
        double microphonicsScale; // Microphonic noise level (Stock=1.0)
    };

    // ------------ Spec tables (constexpr) ------------

    // Op-Amp specs — delegated to OpAmpPrimitive presets
    // TL072CP is the DSP reference baseline (modBlend=0 → all ICs behave like TL072CP).
    // MFG Var slider scales the deviation from TL072CP: 0=no character, 0.5=nominal, 1.0=max.
    static constexpr OpAmpSpec opAmpSpecs[kOpAmpCount] = {
        /* TL072CP  */  OpAmpPrimitive::TL072CP(),
        /* JRC4558D */  OpAmpPrimitive::JRC4558D(),
        /* OPA2134  */  OpAmpPrimitive::OPA2134(),
        /* LM4562   */  OpAmpPrimitive::LM4562(),
        /* NE5532   */  OpAmpPrimitive::NE5532(),
        /* LM741    */  OpAmpPrimitive::LM741(),
    };

    // Compander specs
    // SA571N: noticeably tighter than NE570 (faster attack & release).
    static constexpr CompanderSpec companderSpecs[kCompanderCount] = {
        //                  name      atkScale  relScale  vcaOut  noise
        /* NE570  */    { "NE570",    1.0,      1.0,      0.60,   1.0  },
        /* SA571N */    { "SA571N",   0.50,     0.60,     0.70,   0.45 },
    };

    // Capacitor Grade specs
    // esr: lower = less HF loss in BBD filter → brighter sound. Converted via sqrt(1/esr) to bandwidth scale.
    //   Stock(1.0)→BW×1.0  Film(0.45)→BW×1.49  AudioGrade(0.20)→BW×2.24
    static constexpr CapGradeSpec capGradeSpecs[kCapGradeCount] = {
        //                  name          tolerance  esr   microphonics
        /* Standard  */ { "Standard",     1.0,       1.0,  1.0  },
        /* Film      */ { "Film",         0.35,      0.45, 0.15 },
        /* AudioGrade*/ { "Audio Grade",  0.15,      0.20, 0.03 },
    };

    // Accessors

    const OpAmpSpec&      getOpAmpSpec()      const { return opAmpSpecs[std::clamp(opAmp, 0, (int)kOpAmpCount - 1)]; }
    const CompanderSpec&  getCompanderSpec()  const { return companderSpecs[std::clamp(compander, 0, (int)kCompanderCount - 1)]; }
    const CapGradeSpec&   getCapGradeSpec()   const { return capGradeSpecs[std::clamp(capGrade, 0, (int)kCapGradeCount - 1)]; }

    // Serialization (standalone version)
    // Simple JSON serialization when not using JUCE ValueTree
    std::string toJson() const noexcept {
        return std::string("{\"opAmp\":") + std::to_string(opAmp)
             + ",\"compander\":" + std::to_string(compander)
             + ",\"capGrade\":" + std::to_string(capGrade)
             + "}";
    }

    static ModdingConfig fromJson(const std::string& s) noexcept {
        ModdingConfig cfg;
        auto parseIntField = [&](const char* key, int def)->int{
            const std::string ks(key);
            size_t p = s.find(ks);
            if (p == std::string::npos) return def;
            p = s.find(':', p);
            if (p == std::string::npos) return def;
            ++p;
            while (p < s.size() && std::isspace((unsigned char)s[p])) ++p;
            bool neg = false;
            if (p < s.size() && s[p] == '-') { neg = true; ++p; }
            int val = 0; bool any = false;
            while (p < s.size() && std::isdigit((unsigned char)s[p])) { any = true; val = val * 10 + (s[p] - '0'); ++p; }
            if (!any) return def;
            return neg ? -val : val;
        };

        cfg.opAmp = std::clamp(parseIntField("opAmp", (int)TL072CP), 0, (int)kOpAmpCount - 1);
        cfg.compander = std::clamp(parseIntField("compander", 0), 0, (int)kCompanderCount - 1);
        cfg.capGrade = std::clamp(parseIntField("capGrade", 0), 0, (int)kCapGradeCount - 1);
        return cfg;
    }

    bool operator==(const ModdingConfig& o) const
    {
        return opAmp == o.opAmp && compander == o.compander
            && capGrade == o.capGrade;
    }
    bool operator!=(const ModdingConfig& o) const { return !(*this == o); }
};
