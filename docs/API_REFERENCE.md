# Patina — Function Reference

> All classes live in `dsp/` and require **C++17**.  
> No external dependencies — include `dsp/core/AudioCompat.h` for the
> standalone compat layer (`patina::compat::AudioBuffer`, `MathConstants`, etc.).  
> Language bindings available in `bindings/c/` (C API) and `bindings/rust/` (Rust crate).
>
> **Architecture — 4-Layer Model**:  
> `Constants (L1)` → `Parts (L2)` → `Circuits (L3)` → `Engines (L4)`  
> Each layer depends only on the layers below it; there are no circular dependencies within the same layer.

---

## Table of Contents

### Core
1. [ProcessSpec](#1-processspec)
2. [FastMath](#2-fastmath)

### L1 — Constants
3. [PartsConstants](#3-partsconstants)

### L2 — Parts / Primitives
4. [RC_Element](#4-rc_element)
5. [OTA_Primitive](#5-ota_primitive)
6. [JFET_Primitive](#6-jfet_primitive)
7. [BJT_Primitive](#7-bjt_primitive)
8. [DiodePrimitive](#8-diodeprimitive)
9. [TubeTriode](#9-tubetriode)
10. [TransformerPrimitive](#10-transformerprimitive)
11. [TapePrimitive](#11-tapeprimitive)
12. [PhotocellPrimitive](#12-photocellprimitive)
13. [AnalogVCO](#13-analogvco)
14. [VcaPrimitive](#14-vcaprimitive)
15. [OpAmpPrimitive](#15-opampprimitive)
16. [InductorPrimitive](#16-inductorprimitive)
17. [PowerPentode](#17-powerpentode)
18. [VactrolPrimitive](#18-vactrolprimitive)

### L3 — Circuits

#### L3: Drive
19. [DiodeClipper](#19-diodeclipper)
20. [FrontEnd](#20-frontend)
21. [InputBuffer](#21-inputbuffer)
22. [InputFilter](#22-inputfilter)
23. [OutputStage](#23-outputstage)

#### L3: Filters
24. [ToneFilter / ToneShaper](#24-tonefilter-toneshaper)
25. [StateVariableFilter](#25-statevariablefilter)
26. [LadderFilter](#26-ladderfilter)
27. [PhaserStage](#27-phaserstage)
28. [OtaSKFilter](#28-otaskfilter)
29. [DiodeLadderFilter](#29-diodeladderfilter)
30. [AnalogAllPass](#30-analogallpass)
31. [PassiveLCFilter](#31-passivelcfilter)

#### L3: BBD
32. [BbdStageEmulator](#32-bbdstageemulator)
33. [BbdClock](#33-bbdclock)
34. [BbdNoise](#34-bbdnoise)
35. [BbdTimeController](#35-bbdtimecontroller)
36. [DelayLineView](#36-delaylineview)
37. [BbdPipeline](#37-bbdpipeline)
38. [BbdFeedback](#38-bbdfeedback)

#### L3: Compander / Dynamics
39. [CompanderModule](#39-compandermodule)
40. [DynamicsSuite](#40-dynamicssuite)
41. [EnvelopeFollower](#41-envelopefollower)
42. [TremoloVCA](#42-tremolovca)
43. [NoiseGate](#43-noisegate)
44. [PhotoCompressor](#44-photocompressor)
45. [FetCompressor](#45-fetcompressor)
46. [VariableMuCompressor](#46-variablemucompressor)
47. [VcaCompressor](#47-vcacompressor)

#### L3: Delay / Reverb
48. [OversamplePath](#48-oversamplepath)
49. [SpringReverbModel](#49-springreverbmodel)
50. [PlateReverb](#50-platereverb)

#### L3: Saturation
51. [TubePreamp](#51-tubepreamp)
52. [TransformerModel](#52-transformermodel)
53. [TapeSaturation](#53-tapesaturation)
54. [WaveFolder](#54-wavefolder)
55. [PushPullPowerStage](#55-pushpullpowerstage)

#### L3: Modulation
56. [AnalogLfo](#56-analoglfo)
57. [ModulationBus](#57-modulationbus)
58. [StereoImage](#58-stereoimage)
59. [RingModulator](#59-ringmodulator)
60. [MidSideIron](#60-midsideiron)
61. [MidSidePrecision](#61-midsideprecision)

#### L3: Mixer
62. [Mixer](#62-mixer)
63. [DuckingMixer](#63-duckingmixer)
64. [GainUtils](#64-gainutils)

#### L3: Power
65. [PowerSupplySag](#65-powersupplysag)
66. [BatterySag](#66-batterysag)
67. [AdapterSag](#67-adaptersag)
68. [CapacitorAging](#68-capacitoraging)

### L4 — Engines
69. [BbdDelayEngine](#69-bbddelayengine)
70. [DriveEngine](#70-driveengine)
71. [ReverbEngine](#71-reverbengine)
72. [CompressorEngine](#72-compressorengine)
73. [EnvelopeGeneratorEngine](#73-envelopegeneratorengine)
74. [ModulationEngine](#74-modulationengine)
75. [TapeMachineEngine](#75-tapemachineengine)
76. [ChannelStripEngine](#76-channelstripengine)
77. [EqEngine](#77-eqengine)
78. [LimiterEngine](#78-limiterengine)
79. [FilterEngine](#79-filterengine)
- [Typical Full Chain Example](#typical-full-chain-example)

### Config
80. [ModdingConfig](#80-moddingconfig)
81. [AnalogPresets](#81-analogpresets)

### Language Bindings
82. [C API (patina_c.h)](#82-c-api-patina_ch)
83. [Rust Crate (patina-dsp)](#83-rust-crate-patina-dsp)

---

# Core

---

## 1. ProcessSpec

`#include "dsp/core/ProcessSpec.h"`

Common initialization specification struct shared by all modules. Unifies the argument passed to `prepare()`.

```cpp
namespace patina {
struct ProcessSpec {
    double sampleRate   = 44100.0;
    int    maxBlockSize = 512;
    int    numChannels  = 2;
};
}
```

**Usage:**
```cpp
patina::ProcessSpec spec{48000.0, 256, 2};
compressor.prepare(spec);  // Just pass the same spec to every module
bbdStageEmu.prepare(spec);
toneFilter.prepare(spec);
```

> All modules (DiodeClipper, InputBuffer, InputFilter, OutputStage, AnalogLfo,
> CompanderModule, BbdStageEmulator, BbdSampler, BbdFeedback, ToneFilter) have a
> `prepare(const patina::ProcessSpec&)` overload.
> The existing `prepare(int numChannels, double sampleRate)` remains available as well.

---

## 2. FastMath

`#include "dsp/core/FastMath.h"`

Fast tanh via [7,7] Padé approximation. Maximum error < 2e-7 for `|x| ≤ 3.5`. Approximately 6.7× faster than `std::tanh`.

```cpp
namespace FastMath {
    double fastTanh(double x) noexcept;
    float  fastTanhf(float x) noexcept;

    // NaN / Inf guard — replaces non-finite values with 0
    inline float  sanitize(float x) noexcept;
    inline double sanitize(double x) noexcept;
}
```

| Function | Purpose |
|----------|---------|
| `fastTanh()` / `fastTanhf()` | Fast soft clipping |
| `sanitize()` | Removes NaN/Inf from filter state and input |

---

## 2b. DenormalGuard

`#include "dsp/core/DenormalGuard.h"`

RAII guard class that suppresses denormal numbers. Used at the beginning of the audio thread's `processBlock`.

```cpp
namespace patina {
    class ScopedDenormalDisable {
    public:
        ScopedDenormalDisable() noexcept;   // Enables FTZ/DAZ
        ~ScopedDenormalDisable() noexcept;  // Restores original settings
    };
}
```

| Platform | Method |
|---|---|
| x86 / x86_64 | SSE `_mm_setcsr()` — FTZ (bit 15) + DAZ (bit 6) |
| AArch64 | FPCR FZ bit (bit 24) |
| ARM32 | FPSCR FZ bit (bit 24) |

**Usage Pattern** — already integrated into `processBlock` of all L4 engines:

```cpp
void processBlock(...) {
    if (nCh <= 0 || numSamples <= 0) return;
    ScopedDenormalDisable denormalGuard;  // ← here
    // ... processing ...
}   // ← automatically restored by destructor
```

---

# L1 — Constants

> Provides measured component values from real circuits as compile-time constants. Referenced by all layers.

---

## 3. PartsConstants

`#include "dsp/constants/PartsConstants.h"`

Measured component value constants for classic BBD delay unit circuitry. All values are `constexpr`. Key constants:

| Constant | Value | Meaning |
|---|---|---|
| `bbdStagesDefault` | 8192 | Number of BBD stages (MN3005 × 2) |
| `V_supplyMin` | 9.0 V | Minimum supply voltage |
| `V_supplyMax` | 18.0 V | Maximum supply voltage |
| `R_input` | 1 MΩ | Input impedance |
| `diode_forward_v` | 0.65 V | Silicon diode forward voltage |
| `ne570AttackSec` | ≈ 10.7 ms | NE570 attack time constant |
| `ne570ReleaseSec` | ≈ 81.6 ms | NE570 release time constant |
| `V_ref_NE570` | 316 mVrms | NE570 internal reference level (-10 dBV) |

---

# L2 — Parts / Primitives

> Stateless primitives modeling individual electronic components (resistors, capacitors, semiconductors, vacuum tubes).
> Building blocks for L3 circuit modules.

---

## 4. RC_Element

`#include "dsp/parts/RC_Element.h"`

RC passive element primitive. Constructs a 1st-order LPF / HPF / AllPass from resistance R and capacitance C. Building block for circuit modules.

```cpp
class RC_Element {
public:
    RC_Element() noexcept = default;
    RC_Element(double resistance, double capacitance) noexcept;

    void prepare(double sr) noexcept;
    void reset() noexcept;
    void setRC(double resistance, double capacitance) noexcept;
    double cutoffHz() const noexcept;    // fc = 1 / (2π RC)

    double processLPF(double x) noexcept;  // 1st-order LPF
    double processHPF(double x) noexcept;  // 1st-order HPF
    double processAP(double x) noexcept;   // 1st-order allpass

    double getAlpha() const noexcept;
    double getR() const noexcept;
    double getC() const noexcept;
};
```

**Usage:**
```cpp
RC_Element rc(10e3, 100e-9);  // 10kΩ + 100nF → fc ≈ 159 Hz
rc.prepare(44100.0);

double lpOut = rc.processLPF(input);
double hpOut = rc.processHPF(input);
```

---

## 5. OTA_Primitive

`#include "dsp/parts/OTA_Primitive.h"`

OTA (operational transconductance amplifier) primitive. Models gm characteristics, tanh saturation, temperature dependence, and manufacturing tolerances of LM13700 / CA3080-type devices. Used for integrator steps in filters and phasers.

```cpp
class OTA_Primitive {
public:
    struct Spec {
        double thermalNoise  = 3e-6;    // Input-referred noise
        double tempCoeffGm   = -0.003;  // gm temperature coefficient (/°C)
        double saturationV   = 2.5;     // Saturation voltage (V)
        double mismatchSigma = 0.015;   // Differential pair mismatch σ
    };
    static constexpr Spec LM13700();
    static constexpr Spec CA3080();

    explicit OTA_Primitive(const Spec& s, unsigned int seed = 211) noexcept;

    double saturate(double x) const noexcept;       // tanh gm saturation
    double gmScale(double temperature = 25.0) const noexcept; // Temperature dependent
    double integrate(double input, double state, double gCoeff) const noexcept;
    double getMismatch() const noexcept;
    const Spec& getSpec() const noexcept;
};
```

| Preset | thermalNoise | saturationV | mismatchSigma |
|---|---|---|---|
| `LM13700()` | 3e-6 | 2.5 | 0.015 |
| `CA3080()` | 4e-6 | 2.0 | 0.020 |

---

## 6. JFET_Primitive

`#include "dsp/parts/JFET_Primitive.h"`

JFET primitive. Models VCA nonlinearity, drain current saturation, and temperature-dependent pinch-off of 2N5457 / 2N3819-type devices. Used in phasers and FET compressors.

```cpp
class JFET_Primitive {
public:
    struct Spec {
        double Vp = -3.0;           // Pinch-off voltage
        double vpTempCoeff = -0.002; // Temperature coefficient
        double gmMismatch = 0.08;   // gm variation σ
        double satVoltage = 3.0;    // Saturation voltage
        double noiseLevel = 2e-6;   // Intrinsic noise level
    };
    static constexpr Spec N2N5457();
    static constexpr Spec N2N3819();

    explicit JFET_Primitive(const Spec& s, unsigned int seed = 67) noexcept;

    double softClip(double x) const noexcept;       // Drain current saturation
    double vcaNonlinearity(double x, double gain) const noexcept; // VCA nonlinearity
    double tempFreqScale(double temperature = 25.0) const noexcept;
    double getGmScale() const noexcept;
    const Spec& getSpec() const noexcept;
};
```

---

## 7. BJT_Primitive

`#include "dsp/parts/BJT_Primitive.h"`

Bipolar junction transistor primitive. Provides differential-pair tanh saturation, interstage capacitive coupling, and temperature-dependent Vbe. Used for interstage modeling in transistor ladder filters.

```cpp
class BJT_Primitive {
public:
    struct Spec {
        double mismatchSigma = 0.02;      // Differential pair variation σ
        double tempCoeffVbe = -0.002;      // Vbe temperature coefficient
        double resonanceDamping = 0.5;     // Resonance damping
        double interStageCapAlpha = 0.005; // Interstage capacitive coupling coefficient
        double thermalNoise = 3e-6;        // Thermal noise
    };
    static constexpr Spec Generic();
    static constexpr Spec Matched();

    explicit BJT_Primitive(const Spec& s, unsigned int seed = 123) noexcept;

    static double saturate(double x) noexcept;       // tanh differential pair saturation
    double tempScale(double temperature = 25.0) const noexcept;
    double integrate(double input, double state, double gCoeff) const noexcept;
    double interStageCoupling(double y, double& capState) const noexcept;
    double getMismatch() const noexcept;
    const Spec& getSpec() const noexcept;
};
```

---

## 8. DiodePrimitive

`#include "dsp/parts/DiodePrimitive.h"`

Diode primitive. Presets for Si (1N4148) / Schottky (1N5818) / Ge (OA91) / diode ladder / OTA-SK input protection types. Models temperature-dependent Vf, asymmetric clipping, and junction capacitance.

```cpp
class DiodePrimitive {
public:
    struct Spec {
        double Vf_25C = 0.65;      // Forward voltage at 25°C
        double tempCoeff = -0.002; // Temperature coefficient
        double seriesR = 1.0;      // Series resistance
        double slopeScale = 40.0;  // Exponential slope
        double asymmetry = 1.0;    // Positive/negative asymmetry
        double junctionCap = 4e-12; // Junction capacitance
        double recoveryTime = 4e-9; // Reverse recovery time
    };
    static constexpr Spec Si1N4148();
    static constexpr Spec Schottky1N5818();
    static constexpr Spec GeOA91();
    static constexpr Spec LowVfSilicon();
    static constexpr Spec OtaInputDiode();

    explicit DiodePrimitive(const Spec& s) noexcept;

    double effectiveVf(double temperature = 25.0) const noexcept;
    double saturate(double x, double temperature = 25.0) const noexcept;
    double clip(double x, double temperature = 25.0) const noexcept;
    double feedbackClip(double x, double temperature = 25.0) const noexcept;
    double junctionCapacitance(double voltage) const noexcept;
    const Spec& getSpec() const noexcept;
};
```

| Preset | Vf_25C | seriesR | slopeScale | asymmetry |
|---|---|---|---|---|
| `Si1N4148()` | 0.65 | 1.0 | 40.0 | 1.0 |
| `Schottky1N5818()` | 0.32 | 0.3 | 30.0 | 1.0 |
| `GeOA91()` | 0.25 | 5.0 | 20.0 | 1.15 |

---

## 9. TubeTriode

`#include "dsp/parts/TubeTriode.h"`

Vacuum tube triode primitive. Models plate characteristics, Miller capacitance, plate impedance, and grid conduction of 12AX7 / 12AT7 / 12BH7 tubes. Fundamental building block for preamps and compressors.

```cpp
class TubeTriode {
public:
    struct Spec {
        double plateR = 62500.0;      // Plate resistance
        double plateCap = 3.2e-12;    // Plate capacitance
        double millerCap = 1.7e-12;   // Miller capacitance
        double millerAv = 60.0;       // Miller gain
        double gridR = 68e3;          // Grid resistance
        double micFreq = 180.0;       // Microphonic frequency
        double micLevel = 2e-5;       // Microphonic level
        double shotNoise = 1e-5;      // Shot noise
        double emissionLossMax = 0.4; // Maximum emission loss
    };
    struct VariableMuSpec {
        double muCoeff = 3.0;
        double minGain = 0.001;
        double gainSlewRate = 0.05;
    };
    static constexpr Spec T12AX7();
    static constexpr Spec T12AT7();
    static constexpr Spec T12BH7();
    static constexpr VariableMuSpec Tube6386();

    explicit TubeTriode(const Spec& s) noexcept;

    void prepare(double sr) noexcept;
    void reset() noexcept;

    // Static functions (stateless)
    static double transferFunction(double x, double ageFactor = 1.0) noexcept;
```

> **transferFunction output range**: Positive side is naturally bounded by `tanh` (max ~1.2).
> Negative side is clamped at `-1.0` (fixed in v0.O).
    static double softSaturate(double x) noexcept;
    static double pushPullBalance(double x) noexcept;
    static double outputSaturate(double x) noexcept;
    static double variableMuGain(double controlVoltage, const VariableMuSpec& vmu) noexcept;

    // Stateful filters (single channel)
    double processMiller(double x) noexcept;
    double processPlateImpedance(double x) noexcept;
    double processGridConduction(double x) noexcept;

    const Spec& getSpec() const noexcept;

    // Public coefficients (for external state management)
    double millerAlpha, plateAlpha;
};
```

---

## 10. TransformerPrimitive

`#include "dsp/parts/TransformerPrimitive.h"`

Comprehensive physical model of an audio transformer. Models turns-ratio voltage/impedance conversion, primary inductance LF coupling, leakage inductance HF rolloff, interwinding capacitance resonance, B-H hysteresis magnetic saturation, core loss, temperature-dependent permeability, CMRR, and phase inversion. Handles everything from light saturation processing (console/FET compressor) to full-chain processing (mic preamp input, DI box, tube interstage coupling, ribbon mic output) in a unified manner.

```cpp
class TransformerPrimitive {
public:
    struct Spec {
        // --- Basic properties (for saturation model) ---
        double windingR       = 600.0;     // Winding resistance (Ω)
        double leakageL       = 2e-3;      // Leakage inductance (H)
        double windingCap     = 200e-12;   // Interwinding capacitance (F)
        double coreLossCoeff  = 1e-14;     // Core loss coefficient
        double tempCoeffMu    = -0.001;    // Permeability temperature coefficient (/°C)
        double dcBiasScale    = 0.05;      // DC bias scale
        double resonanceGain  = 0.12;      // Winding resonance gain
        double noiseLevel     = 1e-6;      // Winding thermal noise

        // --- Extended properties (for full-chain model) ---
        double turnsRatio        = 1.0;      // N₂/N₁ turns ratio
        double primaryInductance = 10.0;     // Primary inductance Lp (H)
        double couplingCoeff     = 0.998;    // Coupling coefficient k (0–1)
        double primaryDCR        = 50.0;     // Primary DC resistance (Ω)
        double secondaryDCR      = 50.0;     // Secondary DC resistance (Ω)
        double nominalSourceZ    = 150.0;    // Nominal source Z (Ω)
        double coreSatLevel      = 1.2;      // Core saturation level
        double cmrr_dB           = 80.0;     // CMRR (dB)
        double resonanceQ        = 2.5;      // Resonance Q
        bool   invertPhase       = false;    // Phase inversion
    };

    // Saturation-focused presets (for TransformerModel L3)
    static constexpr Spec BritishConsole();    // British console style
    static constexpr Spec AmericanConsole();   // American console style
    static constexpr Spec CompactFetOutput();  // FET compressor output

    // Full-chain presets
    static constexpr Spec Neve1073Input();     // British mic preamp input 1:10
    static constexpr Spec API2520Output();     // American console output 1:1
    static constexpr Spec Lundahl1538();       // Modern high-end input 1:5
    static constexpr Spec JensenDIBox();       // DI box 10:1
    static constexpr Spec InterstageTriode();  // Tube interstage 1:1 (phase inversion)
    static constexpr Spec RibbonMicOutput();   // Ribbon mic output 1:35

    explicit TransformerPrimitive(const Spec& s) noexcept;

    void prepare(double sr) noexcept;
    void reset() noexcept;

    // Info getters
    double muScale(double temperature = 25.0) const noexcept;
    double impedanceRatio() const noexcept;        // N²
    double getResonanceFreq() const noexcept;
    const Spec& getSpec() const noexcept;

    // Processing
    double process(double input, double satAmount = 0.3, double temperature = 25.0) noexcept;
    double processBalanced(double hot, double cold, double satAmount = 0.3, double temperature = 25.0) noexcept;

    // Individual processing methods
    double processLfCoupling(double x) noexcept;
    double processCoreSaturation(double x, double satAmount, double muScl = 1.0) noexcept;
    double processCoreLoss(double x) noexcept;
    double processResonance(double x) noexcept;
    double processHfRolloff(double x) noexcept;
};
```

**Console / Output stage presets:**

| Preset | windingR | leakageL | Usage |
|---|---|---|---|
| `BritishConsole()` | 600Ω | 2mH | British console magnetic saturation sound |
| `AmericanConsole()` | 400Ω | 1.5mH | American console, tight midrange |
| `CompactFetOutput()` | 300Ω | 1mH | FET limiter output stage sheen |

**Mic preamp / DI / Interstage presets:**

| Preset | Turns Ratio | Lp (H) | Character |
|---|---|---|---|
| `Neve1073Input()` | 1:10 | 10.0 | Even harmonics "silk" from nickel core saturation|
| `API2520Output()` | 1:1 | 5.0 | Tight midrange, clear transients |
| `Lundahl1538()` | 1:5 | 15.0 | Wideband, low distortion, modern high-end |
| `JensenDIBox()` | 10:1 | 140.0 | Hi-Z→Lo-Z conversion, ground loop isolation |
| `InterstageTriode()` | 1:1 | 150.0 | DC isolation + phase inversion, push-pull drive |
| `RibbonMicOutput()` | 1:35 | 0.5 | Ultra-high turns ratio, ribbon-unique harmonics |

---

## 11. TapePrimitive

`#include "dsp/parts/TapePrimitive.h"`

Magnetic tape primitive. Models hysteresis saturation, gap loss, and tape hiss noise for high-speed studio deck / mastering deck configurations.

```cpp
class TapePrimitive {
public:
    struct Spec {
        double gapWidthNew = 5.0;     // Head gap width (when new, μm)
        double baseHissLevel = 5e-5;  // Base hiss level
        double demagDcBias = 0.001;   // Demagnetization DC bias
        double demagNoise = 3e-4;     // Demagnetization noise
    };
    static constexpr Spec HighSpeedDeck();
    static constexpr Spec MasteringDeck();

    explicit TapePrimitive(const Spec& s) noexcept;

    void reset() noexcept;
    double processHysteresis(double x, double satAmount, double coercivity = 1.0) noexcept;
    double gapLossFc(double tapeSpeed, double headWear) const noexcept;
    double generateHiss(double white, double tapeAge) noexcept;
    const Spec& getSpec() const noexcept;
};
```

| Parameter | Description |
|---|---|
| `processHysteresis(x, satAmount, coercivity)` | Magnetic hysteresis saturation. `coercivity` decreases with tape degradation |
| `gapLossFc(tapeSpeed, headWear)` | Calculates head gap loss cutoff frequency |
| `generateHiss(white, tapeAge)` | White noise sample → pink noise hiss (Voss-McCartney) |

---

## 12. PhotocellPrimitive

`#include "dsp/parts/PhotocellPrimitive.h"`

Photocell primitive. Models asymmetric attack/release and CdS memory effect of T4B (CdS LDR + EL panel) / VTL5C3 (LED + LDR). The heart of a photo-opto compressor.

```cpp
class PhotocellPrimitive {
public:
    struct Spec {
        double elAttackMs = 2.0;          // EL panel response (attack)
        double elReleaseMs = 10.0;        // EL panel response (release)
        double cdsAttackMs = 10.0;        // CdS response (attack)
        double cdsReleaseMinMs = 60.0;    // CdS release minimum
        double cdsReleaseMaxMs = 5000.0;  // CdS release maximum (memory effect)
        double historyChargeRate = 0.5;   // Memory effect charge rate
        double historyDischargeRate = 0.05; // Memory effect discharge rate
    };
    static constexpr Spec T4B();     // For opto compressor
    static constexpr Spec VTL5C3();  // Vactrol LED+LDR

    explicit PhotocellPrimitive(const Spec& s) noexcept;

    void prepare(double sr) noexcept;
    void reset() noexcept;
    double process(double scLevel) noexcept;  // Input level → attenuation
    const Spec& getSpec() const noexcept;
};
```

**Usage:**
```cpp
PhotocellPrimitive photocell(PhotocellPrimitive::T4B());
photocell.prepare(44100.0);

double attenuation = photocell.process(sideChainLevel);
// attenuation: 0.0 = unity, 1.0 = maximum attenuation
```

---

## 13. AnalogVCO

`#include "dsp/parts/AnalogVCO.h"`

Audio-rate VCO using BJT differential-pair current source and RC integrator. Can be combined with filters for a complete synthesizer suitable for SuperCollider / Max/MSP UGen construction. Features PolyBLEP anti-aliasing.

```cpp
class AnalogVCO {
public:
    enum class Waveform { Saw = 0, Tri = 1, Pulse = 2 };

    struct Spec {
        double freqHz      = 440.0;  // Base frequency (Hz) 20–20000
        int    waveform    = 0;      // 0=Saw, 1=Tri, 2=Pulse
        double pulseWidth  = 0.5;    // PWM duty cycle 0.05–0.95
        double temperature = 25.0;   // Operating temperature (°C)
        double drift       = 0.002;  // Pitch drift amount 0.0–0.01
    };

    void  prepare(int numChannels, double sampleRate) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, const Spec& spec) noexcept;
    double getPhase(int channel) const noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `freqHz` | 20–20000 | Oscillation frequency |
| `waveform` | 0–2 | 0=Saw, 1=Triangle, 2=Pulse |
| `pulseWidth` | 0.05–0.95 | Pulse wave duty cycle (PWM) |
| `drift` | 0.0–0.01 | Pitch drift from BJT temperature drift |

---

## 14. VcaPrimitive

`#include "dsp/parts/VcaPrimitive.h"`

Model of Blackmer VCA gain cell (THAT 2180 / dbx 2150 type). Reproduces the nonlinear
characteristics of matched BJT pair log/antilog topology (2nd-order THD: from pair mismatch,
3rd-order THD: from BJT saturation). Used as the VCA gain cell in VcaCompressor.

```cpp
class VcaPrimitive {
public:
    struct Spec {
        double pairMismatchSigma = 0.003;  // BJT pair mismatch σ
        double thd3rdCoeff       = 0.0008; // 3rd-order distortion coefficient
        double thd2ndCoeff       = 0.0003; // 2nd-order distortion coefficient
        double thermalNoise      = 1e-6;   // Noise
        double tempCoeffGain     = -0.003; // Temperature coefficient (dB/°C)
        double saturationLevel   = 10.0;   // Output saturation level
    };
    static constexpr Spec THAT2180();  // Classic large-format console bus compressor style
    static constexpr Spec DBX2150();   // Classic American dynamics processor series

    double applyGain(double input, double gainLinear) const noexcept;
    double tempGainScale(double temperature = 25.0) const noexcept;
    double getPairMismatch() const noexcept;
};
```

| Preset | Pair Mismatch σ | THD3rd | THD2nd | Saturation Level | Usage |
|-----------|-----------|--------|--------|-----------|------|
| `THAT2180()` | 0.003 | 0.0008 | 0.0003 | 10.0 | Large-format console bus compressor |
| `DBX2150()` | 0.005 | 0.0012 | 0.0005 | 8.0 | Classic American dynamics processor tone |

---

## 15. OpAmpPrimitive

`#include "dsp/parts/OpAmpPrimitive.h"`

L2 primitive that uniformly models slew rate, saturation, noise, bandwidth limitation, and
manufacturing tolerances of general-purpose op-amp ICs. Used internally by L3/L4 modules such as `BbdFeedback` and `InputBuffer`.

### `struct Spec`

```cpp
struct Spec {
    const char* name          = "TL072CP";
    double slewRate           = 13.0e6;     // Slew rate [V/s]
    double openLoopGain       = 2e5;        // Open-loop gain (Aol)
    double gbwHz              = 3e6;        // Gain-bandwidth product [Hz]
    double satThreshold18V    = 0.88;       // 18V saturation threshold (FS ratio)
    double satCurve18V        = 2.2;        // 18V tanh curve steepness
    double satThreshold9V     = 0.75;       // 9V saturation threshold
    double satCurve9V         = 3.2;        // 9V tanh curve
    double inputNoiseDensity  = 8.0;        // Input noise [nV/√Hz]
    double biasCurrentPA      = 0.065;      // Bias current [pA or nA]
    double noiseScale         = 0.65;       // Relative noise scale
    double offsetDriftUvPerC  = 10.0;       // Offset drift [µV/°C]
    double gbwMismatchSigma   = 0.03;       // GBW manufacturing variation σ
    double asymNeg            = 0.92;       // Negative saturation ratio
};
```

### Preset ICs

| Factory Function | IC | Character |
|---|---|---|
| `TL072CP()` | TL072CP | JFET input dual. Bright and open. Classic for BBD/LFO circuits |
| `JRC4558D()` | JRC4558D | Low slew rate → dark, warm tone. Classic overdrive pedal original |
| `NE5532()` | NE5532 | Console-grade density. Forward midrange |
| `OPA2134()` | OPA2134 | Hi-Fi clean. Wideband, low distortion |
| `LM4562()` | LM4562 | Ultra-low noise, ultra-wideband. Studio-transparent |
| `LM741()` | LM741 | Original general-purpose IC (1968). Narrow bandwidth, high offset. Vintage fuzz "rawness" |

### Methods

```cpp
explicit OpAmpPrimitive(const Spec& s, unsigned int seed = 0) noexcept;
void prepare(double sampleRate) noexcept;
void reset() noexcept;
```

```cpp
double process(double x, bool highVoltage = true) noexcept;
```
Combined processing: slew-rate limiting → saturation (`highVoltage=true` for 18V, `false` for 9V).

```cpp
double saturate(double x, bool highVoltage = true) const noexcept;
```
Soft saturation only (asymmetric positive/negative tanh). Stateless.

```cpp
double applySlewLimit(double x) noexcept;
```
Slew-rate limiting only (stateful — requires `prepare()` + `reset()`).

```cpp
double bandwidthHz(double closedLoopGain = 1.0) const noexcept;
double offsetVoltage(double temperature = 25.0) const noexcept;
double inputNoiseVrms(double bandwidthHz_val = 20000.0) const noexcept;
double fullPowerBandwidthHz(double vPeak = 8.0) const noexcept;
```

**Typical usage:**
```cpp
OpAmpPrimitive amp(OpAmpPrimitive::NE5532());
amp.prepare(44100.0);
double out = amp.process(input);       // Saturation + slew-rate limiting
double sat = amp.saturate(input);      // Saturation only
```

---

## 16. InductorPrimitive

`#include "dsp/parts/InductorPrimitive.h"`

Choke coil / inductor primitive. Physical model of iron-core, ferrite-core, and air-core inductors used in passive LC filters, power supply chokes, and wah-wah pedals.

| Spec field | Default | Unit | Description |
|-----------|---------|------|-------------|
| `inductanceH` | 0.5 | H | Nominal inductance |
| `dcResistance` | 45.0 | Ω | DC winding resistance (DCR) |
| `parasiticCapF` | 100e-12 | F | Parasitic winding capacitance |
| `coreSatLevel` | 1.0 | | Core saturation onset level |
| `coreHystAlpha` | 0.85 | | Hysteresis B-H curve alpha |
| `nominalQ` | 8.0 | | Q factor at 1 kHz |
| `hasFerrite` | false | | Ferrite core (true) vs laminate (false) |
| `tempCoeffMu` | −0.002 | /°C | Permeability temperature coefficient |

**Presets:**

| Preset | Use case | Inductance | DCR | Q |
|--------|----------|-----------|-----|---|
| `HaloInductor()` | Passive EQ toroid | 0.5 H | 45 Ω | 8 |
| `WahInductor()` | Fasel-style wah | 0.5 H | 60 Ω | 4 |
| `PowerChoke()` | Tube amp B+ supply | 10.0 H | 200 Ω | 3 |
| `AirCore()` | Linear (no saturation) | 0.001 H | 2 Ω | 50 |

```cpp
InductorPrimitive(const Spec& s) noexcept;
void prepare(double sampleRate) noexcept;
void reset() noexcept;
double process(double x, float temperature = 25.0f) noexcept;
double muScale(double temperature) const noexcept;
double qAtFreq(double freqHz) const noexcept;
double getSRF() const noexcept;          // Self-resonant frequency
const Spec& getSpec() const noexcept;
```

**Typical usage:**
```cpp
InductorPrimitive ind(InductorPrimitive::HaloInductor());
ind.prepare(48000.0);
double out = ind.process(input, 30.0f);
```

---

## 17. PowerPentode

`#include "dsp/parts/PowerPentode.h"`

Power pentode (beam tetrode) primitive for push-pull power amplifier output stages. Models plate characteristics, crossover distortion, screen grid sag, ultralinear feedback, grid blocking, and thermal compression.

| Spec field | Default | Description |
|-----------|---------|-------------|
| `transconductanceK` | 11.0e-3 | K coefficient in Ip = K(Vg+Vg2/μ₂)^n |
| `exponentN` | 1.5 | Plate law exponent |
| `screenMu` | 22.0 | Screen grid μ₂ |
| `posClipOnset` | 0.75 | Positive saturation onset |
| `negClipOnset` | 0.60 | Negative cutoff onset (asymmetric) |
| `crossoverNotch` | 0.02 | Class AB crossover dead zone |
| `ultralinearTap` | 0.0 | 0=pentode, 0.43=ultralinear, 1=triode |
| `plateDissipationW` | 25.0 | Maximum plate dissipation (W) |

**Presets:**

| Preset | Character | Pdiss | Use case |
|--------|----------|-------|----------|
| `EL34()` | British, aggressive mids | 25 W | Marshall, Hiwatt |
| `_6L6GC()` | American, clean headroom | 30 W | Fender, Mesa |
| `KT88()` | Hi-Fi, massive headroom | 42 W | McIntosh, SVT |
| `EL84()` | Chimey, early breakup | 12 W | Vox AC30 |
| `_6V6GT()` | Sweet singing breakup | 14 W | Fender Deluxe |

```cpp
PowerPentode(const Spec& s) noexcept;
void prepare(double sampleRate) noexcept;
void reset() noexcept;
double process(double gridVoltage, double biasPoint = -0.3) noexcept;
double transferFunction(double vg) const noexcept;
double processGridConduction(double vg) noexcept;
double processScreenSag(double ip) noexcept;
double applyUltralinearFeedback(double ip) const noexcept;
double processThermalCompression(double ip) noexcept;
const Spec& getSpec() const noexcept;
double getThermalState() const noexcept;
```

**Typical usage:**
```cpp
PowerPentode tube(PowerPentode::EL34());
tube.prepare(48000.0);
double plateI = tube.process(gridVoltage, -0.3);
```

---

## 18. VactrolPrimitive

`#include "dsp/parts/VactrolPrimitive.h"`

Vactrol (LED + LDR optocoupler) primitive. Analog voltage-controlled resistance element used in optical compressors, modulators, and tremolo circuits. Differs from PhotocellPrimitive (EL panel) in faster response and LED light source.

| Spec field | Default | Description |
|-----------|---------|-------------|
| `ledAttackMs` | 0.5 | LED turn-on time (ms) |
| `ledReleaseMs` | 2.0 | LED turn-off time (ms) |
| `ldrAttackMs` | 5.0 | LDR resistance fall time (ms) |
| `ldrReleaseMs` | 80.0 | LDR resistance rise time (ms) |
| `ldrGamma` | 0.7 | LDR nonlinearity exponent |
| `memoryChargeRate` | 0.001 | History-dependent charge rate |
| `memoryDischargeRate` | 0.0002 | History-dependent discharge rate |
| `distortionCoeff` | 0.03 | LDR-induced distortion amount |
| `darkResistanceOhms` | 10e6 | LDR resistance when dark (Ω) |
| `litResistanceOhms` | 50.0 | LDR resistance when fully lit (Ω) |

**Presets:**

| Preset | Use case | Attack/Release | Character |
|--------|----------|---------------|-----------|
| `VTL5C3()` | Standard (modular synth) | 5/80 ms | Balanced |
| `VTL5C1()` | Slow compressor | 8/150 ms | Heavy memory |
| `NSL32SR3()` | Fast precision | 1/20 ms | Minimal memory |
| `DIY_Custom()` | Experimental | 3/50 ms | Medium |

```cpp
VactrolPrimitive(const Spec& s) noexcept;
void prepare(double sampleRate) noexcept;
void reset() noexcept;
double process(double controlVoltage, float temperature = 25.0f) noexcept;
double applyAsAttenuator(double signal, double controlVoltage, float temp = 25.0f) noexcept;
double getResistanceOhms() const noexcept;
const Spec& getSpec() const noexcept;
```

**Typical usage:**
```cpp
VactrolPrimitive vac(VactrolPrimitive::VTL5C3());
vac.prepare(48000.0);
double gain = vac.process(controlCV, 25.0f);
double out  = vac.applyAsAttenuator(audio, controlCV);
```

---

# L3 — Circuits

> Functional circuit modules combining L2 parts. Organized in subdirectories under `dsp/circuits/`.

---

## L3: Drive

---

## 19. DiodeClipper

`#include "dsp/circuits/drive/DiodeClipper.h"`

Analog distortion circuit emulator. Pipeline: RC high-pass filter → gain → nonlinear clipping.

### `enum class Mode`
| Value | Description |
|---|---|
| `Bypass` | No nonlinear processing |
| `Diode` | Diode clipping (exponential saturation) |
| `Tanh` | tanh soft saturation |

### `struct Params`
```cpp
struct Params {
    float drive = 0.0f;  // Drive amount 0.0–1.0
    int mode = 0;        // 0=Bypass, 1=Diode, 2=Tanh
};
```

### Methods

```cpp
void prepare(int numChannels, double sr) noexcept;
void prepare(const patina::ProcessSpec& spec) noexcept;
```
Sets channel count and sample rate. Initializes state buffers.

```cpp
void reset() noexcept;
```
Resets all filter states to zero.

```cpp
void setEffectiveSampleRate(double sr) noexcept;
```
Recalculates HP filter coefficients when the effective sample rate changes (e.g., during oversampling).

```cpp
inline float process(int channel, float x, float drive, int modeIndex = 0) noexcept;
```
| Argument | Type | Description |
|---|---|---|
| `channel` | `int` | Channel index (0-based) |
| `x` | `float` | Input sample (full-scale value) |
| `drive` | `float` | Drive amount 0.0–1.0 (internal gain 1×–4×) |
| `modeIndex` | `int` | 0=Bypass, 1=Diode, 2=Tanh |

```cpp
void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept;
```
Block processing. Processes all channels × all samples at once.

**Typical usage:**
```cpp
DiodeClipper drive;
drive.prepare(2, 44100.0);
float out = drive.process(0, inputSample, 0.6f, 1); // Diode mode

// Block processing version
DiodeClipper::Params params{0.6f, 1};
drive.processBlock(io, 2, blockSize, params);
```

---

## 20. FrontEnd

`#include "dsp/circuits/drive/FrontEnd.h"`

Stateless helper struct for the BBD delay engine input stage. Converts full-scale float samples to voltage domain, applies `InputBuffer` (TL072 slew/LPF) and `InputFilter` (LPF cutoff) in a single call. Used internally by `BbdDelayEngine`; exposed for custom engine builders who want to replicate the same front-end chain.

### Key Method

```cpp
static void processOne(float inFS, float fsToVGain, bool isInstrument,
                       InputBuffer& tl072, InputFilter& lpf,
                       int ch, float& outDryV, float& outWetFrontV) noexcept;
```

| Parameter | Description |
|---|---|
| `inFS` | Input sample in full-scale (±1.0) |
| `fsToVGain` | Conversion factor from FS to voltage domain |
| `isInstrument` | `true` → instrument-level headroom; `false` → line level |
| `outDryV` | Output: dry voltage (pre-InputBuffer) |
| `outWetFrontV` | Output: wet voltage (post-InputBuffer + InputFilter) |

---

## 21. InputBuffer

`#include "dsp/circuits/drive/InputBuffer.h"`

Models a TL072 op-amp input buffer. 1MΩ input impedance, 1-pole LPF from cable capacitance, supply-voltage dependent headroom, optional -20dB PAD.
Slew-rate limiting is delegated to per-channel `OpAmpPrimitive` instances.

### Methods

```cpp
void prepare(int numChannels, double sr) noexcept;
void prepare(const patina::ProcessSpec& spec) noexcept;
void reset() noexcept;
```

```cpp
void setPadEnabled(bool en) noexcept;
```
Set to `true` to enable -20dB PAD (Line → Instrument level conversion).

```cpp
void setInputCapacitance(double cFarads) noexcept;
```
Sets cable capacitance (1pF–5nF). Higher values lower the LPF cutoff.

```cpp
void setSupplyVoltage(double v) noexcept;
```
Sets supply voltage to `9.0` or `18.0` V. Affects headroom.

```cpp
void setHeadroomKnees(double knee9V, double knee18V) noexcept;
```
Sets the soft-saturation onset point per supply voltage (0.1–1.0).

```cpp
void setSlewRate(double vrPerSec) noexcept;
```
Sets slew rate (in V/s). JRC4558D=0.5e6, TL072=13e6. Rebuilds `OpAmpPrimitive` instances.

```cpp
inline float process(int ch, float x) noexcept;
```
Per-sample processing. PAD → RC LPF → headroom saturation.

```cpp
bool consumeOverloadFlag() const noexcept;
```
Reads and clears the overload detection flag.

```cpp
void processBlock(float* const* io, int numChannels, int numSamples) noexcept;
```
Block processing. Processes all channels × all samples at once.

---

## 22. InputFilter

`#include "dsp/circuits/drive/InputFilter.h"`

2-stage cascaded 1-pole IIR low-pass filter (with digital LPF stage). Default cutoff ≈ 4547 Hz.

### Methods

```cpp
void prepare(int numChannels, double sr) noexcept;
void prepare(const patina::ProcessSpec& spec) noexcept;
void reset() noexcept;
```

```cpp
void setCutoffHz(double fcDesired) noexcept;
```
Sets overall -3dB cutoff frequency in Hz. Applies N-stage cascade correction automatically.

```cpp
void setDefaultCutoff() noexcept;
```
Resets to `PartsConstants::inputLpfFcHz`.

```cpp
inline float process(int ch, float x) noexcept;
inline void processStereo(float& ch0, float& ch1) noexcept;
```
Scalar or simultaneous channel 0/1 processing.

```cpp
void processBlock(float* const* io, int numChannels, int numSamples) noexcept;
```
Block processing. Processes all channels × all samples at once.

---

## 23. OutputStage

`#include "dsp/circuits/drive/OutputStage.h"`

3-pole LPF (-18dB/oct) + supply-voltage-dependent tanh soft saturation. Models the analog output filter of a BBD delay.

### Methods

```cpp
void prepare(int numChannels, double sr) noexcept;
void prepare(const patina::ProcessSpec& spec) noexcept;
void reset() noexcept;
```

```cpp
void setCutoffHz(double fc) noexcept;
```
Sets the common cutoff frequency for all 3 stages in Hz.

```cpp
inline float process(int channel, float x, double supplyVoltage) noexcept;
```
| Argument | Description |
|---|---|
| `x` | Input sample |
| `supplyVoltage` | Supply voltage (V). Affects headroom and saturation characteristics. |

**Returns:** Sample after 3-pole LPF → tanh saturation processing.

```cpp
void processBlock(float* const* io, int numChannels, int numSamples, double supplyVoltage) noexcept;
```
Block processing. Processes all channels × all samples at once.

---

## L3: Filters

---

## 24. ToneFilter / ToneShaper

`#include "dsp/circuits/filters/ToneFilter.h"`  
`#include "dsp/circuits/filters/ToneShaper.h"`

### ToneFilter
Per-channel IIR LPF bank. Calculates cutoff values by emulating RC circuit behavior from tone knob value.

```cpp
void prepare(double sampleRate, int channels, int maxBlock);
void prepare(const patina::ProcessSpec& spec);
void reset();
void ensureChannels(int channels, int maxBlock);
```

```cpp
void setDefaultCutoff(float cutoffHz = 2000.0f);
void updateToneFilter(float cutoffHz = 2000.0f);
```

```cpp
void updateToneFilterIfNeeded(float toneParam, double agingCapScale, bool emulateToneRC);
```
| Argument | Description |
|---|---|
| `toneParam` | 0.0–1.0 (0=dark, 1=bright) |
| `agingCapScale` | Capacitor degradation coefficient (1.0 = new) |
| `emulateToneRC` | `true` to calculate RC from pot resistance |

```cpp
inline float processSample(int ch, float x);
```

```cpp
void processBlock(float* const* io, int numChannels, int numSamples);
```
Block processing. Processes all channels × all samples at once.

### ToneShaper
Utility that applies `ToneFilter` to multi-channel `std::vector<float>`.

```cpp
static void process(ToneFilter& bank, int numChannels, std::vector<float>& delayedOut);
```

---

## 25. StateVariableFilter

`#include "dsp/circuits/filters/StateVariableFilter.h"`

OTA-based 2nd-order State Variable Filter (classic OTA filter chip style). Andy Simper's trapezoidal integration model.
Simultaneously outputs LP / HP / BP / Notch.

```cpp
class StateVariableFilter {
public:
    enum class Type { LowPass = 0, HighPass = 1, BandPass = 2, Notch = 3 };

    struct Params {
        float cutoffHz  = 1000.0f;
        float resonance = 0.5f;   // 0.0–1.0
        int   type      = 0;      // 0=LP, 1=HP, 2=BP, 3=Notch
    };

    struct Output { float lp; float hp; float bp; float notch; };

    void prepare(int numChannels, double sr) noexcept;
    void prepare(const patina::ProcessSpec& spec) noexcept;
    void reset() noexcept;

    void setCutoffHz(float hz) noexcept;
    void setResonance(float r) noexcept;  // 0.0–1.0 → Q: 0.5–20.0

    Output processAll(int channel, float x) noexcept;           // All 4 types simultaneous output
    float  process(int channel, float x, int typeIndex=0) noexcept;
    void   processBlock(float* const* io, int numCh, int numSamples,
                        const Params& params) noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `cutoffHz` | 20–20000 Hz | Cutoff frequency |
| `resonance` | 0.0–1.0 | Resonance (internal Q: 0.5–20.0) |
| `type` | 0–3 | LP / HP / BP / Notch selection |

---

## 26. LadderFilter

`#include "dsp/circuits/filters/LadderFilter.h"`

Transistor 4-pole (-24 dB/oct) ladder filter. Huovilainen / Välimäki model.
Each stage uses `tanh` nonlinearity; self-oscillation at resonance=1.0.

```cpp
class LadderFilter {
public:
    struct Params {
        float cutoffHz  = 1000.0f;
        float resonance = 0.0f;    // 0.0–1.0
        float drive     = 0.0f;    // 0.0–1.0
    };

    void prepare(int numChannels, double sr) noexcept;
    void prepare(const patina::ProcessSpec& spec) noexcept;
    void reset() noexcept;

    void  setCutoffHz(float hz) noexcept;
    void  setResonance(float r) noexcept;
    float process(int channel, float x, float driveAmount=0.0f) noexcept;
    void  processBlock(float* const* io, int numCh, int numSamples,
                       const Params& params) noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `cutoffHz` | 20–20000 Hz | Cutoff frequency |
| `resonance` | 0.0–1.0 | Resonance (self-oscillation at 1.0) |
| `drive` | 0.0–1.0 | Input saturation amount |

---

## 27. PhaserStage

`#include "dsp/circuits/filters/PhaserStage.h"`

Classic analog phaser-style JFET allpass filter. 2–12 stages (even numbers recommended).
Center frequency modulated by external LFO input.

```cpp
class PhaserStage {
public:
    struct Params {
        float lfoValue      = 0.0f;     // -1.0–1.0
        float depth         = 0.5f;     // 0.0–1.0
        float feedback      = 0.0f;     // 0.0–0.95
        float centerFreqHz  = 1000.0f;
        float freqSpreadHz  = 800.0f;
        int   numStages     = 4;        // 2–12 (even recommended)
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(float* const* io, int numCh, int numSamples,
                       const Params& params) noexcept;
};
```

---

## 28. OtaSKFilter

`#include "dsp/circuits/filters/OtaSKFilter.h"`

OTA (LM13700) Sallen-Key filter emulation. Features 2-pole (-12dB/oct) LPF / HPF / BPF (HPF→LPF cascade). Models OTA input diode clipping, temperature-dependent gm drift, and component tolerances.

```cpp
class OtaSKFilter {
public:
    enum class Mode : int { LowPass=0, HighPass=1, BandPass=2 };
    struct Params {
        float cutoffHz=1000, resonance=0, drive=0, temperature=25; int mode=0;
    };
    void prepare(int numChannels, double sampleRate);
    void prepare(const patina::ProcessSpec&);
    void reset();
    void setCutoffHz(float hz);
    void setResonance(float r);          // 0–1, 1.0=self-oscillation
    void setMode(Mode m);
    float process(int ch, float x, int modeOverride=-1);
    float process(int ch, float x, const Params&);
    void processBlock(float*const*, int ch, int n, const Params&);
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `cutoffHz` | 20–Nyquist | Cutoff frequency |
| `resonance` | 0.0–1.0 | Resonance (self-oscillation at 1.0) |
| `drive` | 0.0–1.0 | OTA input overdrive |
| `temperature` | °C | gm temperature coefficient drift |

---

## 29. DiodeLadderFilter

`#include "dsp/circuits/filters/DiodeLadderFilter.h"`

Diode ladder filter. 3-pole (-18dB/oct) LPF. Models asymmetric harmonic structure from per-stage diode Vf variations, diode clipping in the feedback path, and temperature-dependent Vf shift.

```cpp
class DiodeLadderFilter {
public:
    struct Params {
        float cutoffHz=1000, resonance=0, drive=0, temperature=25;
    };
    void prepare(int numChannels, double sampleRate);
    void prepare(const patina::ProcessSpec&);
    void reset();
    void setCutoffHz(float hz);
    void setResonance(float r);
    float process(int ch, float x, float drive=0);
    float process(int ch, float x, const Params&);
    void processBlock(float*const*, int ch, int n, const Params&);
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `cutoffHz` | 20–Nyquist | Cutoff frequency |
| `resonance` | 0.0–1.0 | Resonance |
| `drive` | 0.0–1.0 | Input overdrive |
| `temperature` | °C | Diode Vf temperature shift |

---

## 30. AnalogAllPass

`#include "dsp/circuits/filters/AnalogAllPass.h"`

1st / 2nd-order analog allpass filter. Flat frequency response, phase-only modification. Building block for phasers and phase EQ.

```cpp
class AnalogAllPass {
public:
    enum class Order : int { First=1, Second=2 };
    struct Params { float cutoffHz=1000, q=0.707; int order=1; };
    void prepare(int numChannels, double sampleRate);
    void prepare(const patina::ProcessSpec&);
    void reset();
    void setCutoffHz(float hz);
    void setQ(float q);                 // 2nd-order only
    void setOrder(int ord);             // 1 or 2
    float process(int ch, float x);
    float process(int ch, float x, const Params&);
    void processBlock(float*const*, int ch, int n, const Params&);
    double getPhaseAtFreq(double hz) const;  // Phase shift (rad)
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `cutoffHz` | 20–Nyquist | Phase-shift center frequency |
| `q` | 0.1–20.0 | Q value (2nd-order only) |
| `order` | 1, 2 | Filter order |

---

## 31. PassiveLCFilter

`#include "dsp/circuits/filters/PassiveLCFilter.h"`

Passive LC (inductor-capacitor) filter using physical models for both components. Accurately represents classic studio passive equalizer circuits, resonant filters, and vintage crossover networks. The inductor model includes core saturation, DC resistance losses, and parasitic resonance for authentic analog behavior.

**Parts used:** `InductorPrimitive` × 1 + `RC_Element` × 1

```cpp
class PassiveLCFilter {
public:
    enum FilterType { LPF=0, HPF=1, BPF=2, Notch=3 };
    struct Params {
        float cutoffHz    = 1000.0f;
        float resonance   = 0.5f;    // 0.0–1.0
        float drive       = 0.0f;    // Inductor saturation 0.0–1.0
        float temperature = 25.0f;
        int   filterType  = LPF;
    };
    PassiveLCFilter() noexcept;
    explicit PassiveLCFilter(const InductorPrimitive::Spec&) noexcept;
    void  prepare(int numChannels, double sampleRate) noexcept;
    void  prepare(const patina::ProcessSpec&) noexcept;
    void  reset() noexcept;
    void  setCutoffHz(float hz) noexcept;
    void  setResonance(float r) noexcept;
    float process(int ch, float x, int filterType=LPF,
                  float drive=0.0f, float temperature=25.0f) noexcept;
    float process(int ch, float x, const Params&) noexcept;
    void  processBlock(float*const*, int numCh, int numSamples,
                       const Params&) noexcept;
    double getResonantFreqHz() const noexcept;
    double getEffectiveQ() const noexcept;
    const InductorPrimitive& getInductor() const noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `cutoffHz` | 20–Nyquist | Center/cutoff frequency |
| `resonance` | 0.0–1.0 | Q / resonance (limited by inductor Q) |
| `drive` | 0.0–1.0 | Inductor core saturation amount |
| `filterType` | 0–3 | LPF, HPF, BPF, Notch |

**Typical usage:**
```cpp
PassiveLCFilter filt(InductorPrimitive::HaloInductor());
filt.prepare(2, 48000.0);
filt.setCutoffHz(800.0f);
PassiveLCFilter::Params p;
p.resonance = 0.7f;
p.drive = 0.2f;
p.filterType = PassiveLCFilter::BPF;
float out = filt.process(0, input, p);
```

---

## L3: BBD

---

## 32. BbdStageEmulator

`#include "dsp/circuits/bbd/BbdStageEmulator.h"`

Coloring processor that models the tonal characteristics of a BBD (Bucket Brigade Device).

> **Note:** This class does not hold a delay buffer. Use [DelayLineView](#25-delaylineview) for delay readout.  
> The `step` argument of `process()` is currently unused (reserved for future extensions).

### `struct Params`
```cpp
struct Params {
    int stages = PartsConstants::bbdStagesDefault;
    double supplyVoltage = 9.0;
    bool enableAging = false;
    double ageYears = 0.0;
};
```

### Methods

```cpp
void prepare(int numChannels, double sr) noexcept;
void prepare(const patina::ProcessSpec& spec) noexcept;
void reset() noexcept;
```

```cpp
void process(std::vector<float>& samples, double step, int stages,
             double supplyVoltage, bool enableAging, double ageYears) noexcept;
```
| Argument | Description |
|---|---|
| `samples` | **In/out** `samples[ch]` format (element count = number of channels). **1 sample** |
| `step` | Currently unused (future: clock step) |
| `stages` | Number of BBD stages (e.g., `PartsConstants::bbdStagesDefault` = 8192) |
| `supplyVoltage` | Supply voltage (V). Affects LPF bandwidth and saturation |
| `enableAging` | `true` enables dielectric absorption (aging) effects |
| `ageYears` | Number of aging years (ignored when `enableAging=false`) |

```cpp
void setBaseAlphas(double a, double a2) noexcept;
void setBandwidthScale(double s) noexcept; // 0.5–3.0
```

**Typical usage (combined with DelayLineView):**
```cpp
// Per-sample
std::vector<float> ch(1);
ch[0] = DelayLineView::readFromDelay(ringBuf, writePos, 0, delaySamples, stages, true);
bbd.process(ch, 0.0, PartsConstants::bbdStagesDefault, 9.0, false, 0.0);
float out = ch[0];
```

```cpp
void processBlock(float* const* io, int numChannels, int numSamples, double step, const Params& params) noexcept;
```
Block processing. Parameters are specified collectively via the Params struct.

---

## 33. BbdClock

`#include "dsp/circuits/bbd/BbdClock.h"`

Stateless helper struct for BBD delay time and chorus clock calculations. Converts time (ms) and chorus depth parameters into delay-sample counts and BBD clock-modulation step values.

### Key Methods

```cpp
// Effective delay with chorus read-position modulation (minimum 1.5 samples)
static double effectiveDelaySamples(double baseDelaySamples,
                                    double chorusDepthSamples,
                                    double sinvVal) noexcept;

// BBD step reflecting clock-scale modulation by triangle LFO
static double stepWithClock(double baseStep, double triVal,
                            float chorusEff, double kMaxClockModPct) noexcept;

// Convert chorus depth from ms to sample count
static double chorusDepthSamples(double chorusEff, double timeMs,
                                 double sampleRate, double depthMsMax,
                                 double fractionCap = 0.45) noexcept;
```

---

## 34. BbdNoise

`#include "dsp/circuits/bbd/BbdNoise.h"`

Stateless helper struct for BBD feedback signal shaping and noise injection. Wraps `BbdFeedback::process()` and adds op-amp thermal noise + BBD HF noise (high-pass colored) after the sample-and-hold stage.

### Key Methods

```cpp
// Op-amp saturation + noise shaping on the feedback path
static float processFeedback(int ch, float rawFb, BbdFeedback& fbMod,
                             std::minstd_rand& rng,
                             std::normal_distribution<double>& normalDist,
                             double opAmpNoiseGain, double bbdNoiseLevel,
                             double sampleRate,
                             bool highVoltageMode = false,
                             double capacitanceScale = 1.0) noexcept;

// Add thermal + HF noise to write sample after S/H
static void addNoiseAfterSample(int ch, float& writeSample,
                                bool enableAging, ...) noexcept;
```

---

## 35. BbdTimeController

`#include "dsp/circuits/bbd/BbdTimeController.h"`

Resolves BBD stage count and clock frequency from a time-ms parameter, operating mode, and sample rate. Handles Simple / Classic / Expert mode branching and stage-count smoothing.

### `struct Result`

```cpp
struct Result {
    double desiredDelaySamples;  // Requested delay in samples
    double actualDelaySamples;   // Actual delay after stage quantization
    int    stagesForProcessing;  // Stage count for BbdStageEmulator
    double bbdClockHz;           // BBD clock frequency (Hz)
    int    lastSmoothedStagesNext; // Smoothed stage count for next block
};
```

### Key Method

```cpp
Result resolve(double timeMs, int modeIndex, int stagesParam,
               double sampleRate, int lastSmoothedStages,
               bool emulateBBD, double clockJitterStd,
               int classicStagesParam = 2) const noexcept;
```

| `modeIndex` | Mode | Stage source |
|---|---|---|
| 0 | Simple | Auto from TIME with smoothing |
| 1 | Classic | Fixed preset (1024 / 2048 / 4096 / 8192) |
| 2 | Expert | Direct from `stagesParam` |

---

## 36. DelayLineView

Delay readout utility from a ring buffer (`patina::compat::AudioBuffer<float>`). Does not own the buffer.

### Static Methods

```cpp
static float readFromDelay(
    const patina::compat::AudioBuffer<float>& buffer,
    int writePos,
    int channel,
    double delaySamples,
    int stages,
    bool emulateBBD);
```
| Argument | Description |
|---|---|
| `buffer` | Ring buffer (externally owned) |
| `writePos` | Current write position |
| `channel` | Channel index |
| `delaySamples` | Delay amount (in samples, fractional OK) |
| `stages` | BBD stage count (used for S&H quantization) |
| `emulateBBD` | `true` enables S&H quantization |

**Returns:** Linearly interpolated or S&H quantized delayed sample.

```cpp
static float readFromOversampleDelay(
    const patina::compat::AudioBuffer<float>& osBuffer,
    int oversampleWritePos,
    int channel,
    double delaySamples,
    bool emulateBBD);
```
Readout for oversample path only (linear interpolation only).

---

## 37. BbdPipeline

`#include "dsp/circuits/bbd/BbdPipeline.h"`

Utility that applies `BbdStageEmulator` to delayed signal read by `DelayLineView`, with fallback on extreme attenuation.

### Static Method

```cpp
static void process(
    std::vector<float>& delayedOut,
    std::vector<float>& preLpfTmp,
    BbdStageEmulator& emulator,
    double step,
    int stagesForProcessing,
    double effectiveSupplyV,
    bool enableAging,
    double ageYears);
```
- `delayedOut`: In/out (1 sample per channel)
- `preLpfTmp`: Temporary buffer (same size, allocated by caller)

---

## 38. BbdFeedback

`#include "dsp/circuits/bbd/BbdFeedback.h"`

Analog delay feedback path model. DC removal HP → 1st-order allpass filter (phase rotation) → parasitic capacitance LPF → op-amp nonlinearity → noise injection.

Op-amp slew-rate limiting (Step 4a) and saturation emulation (Step 4c) are delegated to per-channel `OpAmpPrimitive` instances.

### Type Alias
```cpp
using OpAmpOverrides = OpAmpPrimitive::Spec;  // Backward-compatible alias
```

### `struct Config`
```cpp
struct Config {
    bool emulateOpAmpSaturation = true;
    double opAmpNoiseGain = 1.0;
    double bbdNoiseLevel = PartsConstants::bbdBaseNoise;
    bool highVoltageMode = false;
    double capacitanceScale = 1.0;
};
```

### Methods

```cpp
void prepare(int numChannels, double sr) noexcept;
void prepare(const patina::ProcessSpec& spec) noexcept;
void reset() noexcept;
```

```cpp
inline float process(
    int channel, float rawFb,
    bool emulateOpAmpSaturation,
    std::minstd_rand& rng,
    std::normal_distribution<double>& normalDist,
    double opAmpNoiseGain, double bbdNoiseLevel,
    double currentSampleRate,
    bool highVoltageMode = false,
    double capacitanceScale = 1.0) noexcept;
```
| Argument | Description |
|---|---|
| `rawFb` | Feedback input sample |
| `emulateOpAmpSaturation` | Enables op-amp saturation emulation |
| `rng` | Random number generator (`std::minstd_rand`). Held by caller |
| `normalDist` | Normal distribution (`std::normal_distribution<double>`). Held by caller |
| `highVoltageMode` | `true` = 18V operating mode (larger headroom) |
| `capacitanceScale` | Capacitor degradation scale (1.0 = standard) |

```cpp
void processBlock(float* const* io, int numChannels, int numSamples,
                  const Config& config,
                  std::minstd_rand& rng, std::normal_distribution<double>& normalDist) noexcept;
```
Block processing. Parameters are specified collectively via the Config struct.

```cpp
void setOpAmpOverrides(const OpAmpPrimitive::Spec& s) noexcept;
```
Replaces the op-amp Spec. Hot-swaps internal `OpAmpPrimitive` instances and re-calls `prepare()`.

---

## L3: Compander / Dynamics

---

## 39. CompanderModule

`#include "dsp/circuits/compander/CompanderModule.h"`

NE570/SA571 compander IC emulator. Handles pre-BBD compression and post-BBD expansion. Features RMS envelope detection, soft knee, and pre/de-emphasis filters.

### `struct Config`
```cpp
struct Config {
    float thresholdVrms;        // Compression threshold (Vrms)
    float sidechainRefVrms;     // Sidechain reference (Vrms)
    float ratio;                // Compression ratio 1.0–4.0
    float kneeDb;               // Soft knee width dB (0–12)
    bool preDeEmphasis;         // Pre/de-emphasis enabled
    double preDeEmphasisFcHz;   // Pre/de-emphasis fc (Hz)
    double preEmphasisRcScale;  // RC scale
    double deEmphasisRcScale;   // RC scale
    double preEmphasisMakeupGain;
    double deEmphasisMakeupGain;
    double vcaOutputRatio;      // VCA output efficiency (NE570=0.6)
    TimingConfig timing;
};
```
### `struct TimingConfig`
```cpp
struct TimingConfig {
    double compressorAttack;   // seconds (default ≈ 10.7 ms)
    double compressorRelease;  // seconds (default ≈ 81.6 ms)
    double expanderAttack;     // seconds
    double expanderRelease;    // seconds
    double rmsTimeConstant;    // seconds (default ≈ 10 ms)
};
```

### Methods

```cpp
void prepare(int numChannels, double sampleRate);
void prepare(const patina::ProcessSpec& spec);
void reset();
```

```cpp
void setConfig(const Config& cfg) noexcept;
Config getConfig() const noexcept;
```
Sets/gets all parameters at once via the Config struct.

```cpp
void setThresholdVrms(float vrms) noexcept;   // Compression threshold (Vrms)
void setSidechainRefVrms(float vrms) noexcept; // Sidechain reference (Vrms)
void setRatio(float r) noexcept;              // Compression ratio 1.0–4.0 (default 2.0)
void setKnee(float k) noexcept;               // Soft knee width dB (0–12)
```

```cpp
void setPreDeEmphasis(bool enable, double fcHz = 3000.0) noexcept;
```
Enables pre/de-emphasis filter and sets fc (200–12000 Hz).

```cpp
void setTimingConfig(const TimingConfig& config) noexcept;
TimingConfig getTimingConfig() const noexcept;
```

```cpp
void setVcaOutputRatio(double ratio) noexcept; // 0.3–1.0 (NE570=0.6)
```

```cpp
float processCompress(int channel, float input, float compAmount,
                      float* appliedGain = nullptr);
```
BBD pre-compression processing. `compAmount` 0.0–1.0 adjusts effect amount. Optionally writes applied gain to `appliedGain`.

```cpp
float processExpand(int channel, float input, float compAmount);
float processExpand(int channel, float input, float compAmount,
                    float detectorSample, float linkedGainOverride = NaN);
```
BBD post-expansion processing. The 2nd overload allows sidechain input and external gain override.

**Typical usage:**
```cpp
CompanderModule comp;
comp.prepare(2, 44100.0);

// Pre-BBD
float compressed = comp.processCompress(ch, input, 0.7f);
// ... BBD delay processing ...
// Post-BBD
float expanded = comp.processExpand(ch, delayed, 0.7f);
```

```cpp
void processBlockCompress(float* const* io, int numChannels, int numSamples, float compAmount);
void processBlockExpand(float* const* io, int numChannels, int numSamples, float compAmount);
```
Block processing (compression/expansion respectively).

---

## 40. DynamicsSuite

`#include "dsp/circuits/compander/DynamicsSuite.h"`

Stateless helper struct for HF-weighted makeup gain control after the compander expand stage in the BBD delay path. Measures full-band and HF RMS of the delayed signal and smoothly adjusts a makeup gain to track a target wet RMS, compensating for the level change introduced by compander aging and HF rolloff.

### Key Method

```cpp
static void process(
    std::vector<float>& delayedOut, int numChannels, double sampleRate,
    double targetWetRms,
    std::vector<double>& hfHpXPrev, std::vector<double>& hfHpYPrev,
    double delayedRmsAlpha, double hfRmsAlpha, double makeupSmoothing,
    double& smoothedDelayedRmsSq, double& smoothedDelayedHfRmsSq,
    double& currentMakeupGain,
    std::atomic<double>& baselineDelayedRms,
    std::atomic<double>& baselineDelayedHfRms,
    bool enableAging, bool& prevEnableAging) noexcept;
```

All state variables are passed by reference; `DynamicsSuite` itself is stateless. Used internally by `BbdDelayEngine`.

---

## 41. EnvelopeFollower

`#include "dsp/circuits/compander/EnvelopeFollower.h"`

Classic envelope filter-style envelope follower. Peak / RMS detection modes.
Outputs 0.0–1.0 control signal, used for modulating filters and effects.

```cpp
class EnvelopeFollower {
public:
    enum class DetectionMode { Peak = 0, RMS = 1 };

    struct Params {
        float attackMs    = 5.0f;
        float releaseMs   = 50.0f;
        float sensitivity = 1.0f;    // Scaling coefficient
        int   mode        = 0;       // 0=Peak, 1=RMS
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;

    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(const float* const* input, float* const* output,
                       float* controlOut, int numCh, int numSamples,
                       const Params& params) noexcept;
    float getEnvelope(int channel) const noexcept;
};
```

---

## 42. TremoloVCA

`#include "dsp/circuits/compander/TremoloVCA.h"`

3-mode tremolo / VCA: classic American-style bias tremolo, optical (vactrol), and linear VCA.
Amplitude modulated by external LFO input. Stereo phase-invert option.

```cpp
class TremoloVCA {
public:
    enum class Mode { Bias = 0, Optical = 1, VCA = 2 };

    struct Params {
        float depth            = 0.5f;    // 0.0–1.0
        float lfoValue         = 0.0f;    // -1.0–1.0 (external LFO value)
        int   mode             = 0;       // 0=Bias, 1=Optical, 2=VCA
        bool  stereoPhaseInvert = false;
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(float* const* io, int numCh, int numSamples,
                       const Params& params) noexcept;
};
```

| Mode | Character |
|--------|------|
| `Bias` | Classic American-style bias tremolo. Soft, uneven modulation |
| `Optical` | Vactrol type. Asymmetric attack/release character |
| `VCA` | Linear VCA. Clean, precise amplitude modulation |

---

## 43. NoiseGate

`#include "dsp/circuits/compander/NoiseGate.h"`

Classic studio noise gate-style processor.
Hysteresis open/close, sidechain HPF, and hold time.

```cpp
class NoiseGate {
public:
    struct Params {
        float thresholdDb  = -40.0f;   // dB
        float hysteresisDb =   6.0f;   // dB
        float attackMs     =   0.5f;
        float holdMs       =  50.0f;
        float releaseMs    = 100.0f;
        float range        =   1.0f;   // 0.0–1.0 (degree of full muting)
        float sidechainHpHz = 100.0f;  // Sidechain HPF frequency
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(float* const* io, int numCh, int numSamples,
                       const Params& params) noexcept;
    bool  isGateOpen(int channel) const noexcept;
};
```

---

## 44. PhotoCompressor

`#include "dsp/circuits/dynamics/PhotoCompressor.h"`

Photo-coupled opto compressor. Models program-dependent behavior of T4B photocell (CdS LDR + EL panel). Extended release after sustained compression due to memory effect.

```cpp
class PhotoCompressor {
public:
    enum class Mode : int { Compress=0, Limit=1 };
    struct Params {
        float peakReduction=0.5, outputGain=0.5; int mode=0; float mix=1;
    };
    void prepare(int numChannels, double sampleRate);
    void prepare(const patina::ProcessSpec&);
    void reset();
    float process(int ch, float x, const Params&);
    void processBlock(float*const*, int ch, int n, const Params&);
    float getGainReductionDb(int ch) const;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `peakReduction` | 0.0–1.0 | Sidechain sensitivity (photo-opto peak reduction) |
| `outputGain` | 0.0–1.0 | Makeup gain |
| `mode` | 0/1 | Compress=soft knee, Limit=hard knee |
| `mix` | 0.0–1.0 | Dry/Wet parallel compression |

---

## 45. FetCompressor

`#include "dsp/circuits/dynamics/FetCompressor.h"`

FET compressor. Ultra-fast attack (20μs) via JFET VCA (2N5457), Class-A output transformer coloration, and All-Buttons nuke mode.

```cpp
class FetCompressor {
public:
    enum class Ratio : int { R4to1=0, R8to1=1, R12to1=2, R20to1=3, All=4 };
    struct Params {
        float inputGain=0.5, outputGain=0.5, attack=0.5, release=0.5;
        int ratio=0; float mix=1;
    };
    void prepare(int numChannels, double sampleRate);
    void prepare(const patina::ProcessSpec&);
    void reset();
    float process(int ch, float x, const Params&);
    void processBlock(float*const*, int ch, int n, const Params&);
    float getGainReductionDb(int ch) const;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `inputGain` | 0.0–1.0 | Input gain → controls compression amount |
| `outputGain` | 0.0–1.0 | Makeup gain |
| `attack` | 0.0–1.0 | Attack (20μs–800μs) |
| `release` | 0.0–1.0 | Release (50ms–1100ms) |
| `ratio` | 0–4 | 4:1, 8:1, 12:1, 20:1, All-Buttons |

---

## 46. VariableMuCompressor

`#include "dsp/circuits/dynamics/VariableMuCompressor.h"`

Variable-mu tube compressor. Exponential gain characteristics via remote-cutoff tube (6386), push-pull balanced topology (even-harmonic cancellation), and 6-position time constant selector.
Control voltage computed in dB domain with 6dB soft knee for a natural compression curve.

```cpp
class VariableMuCompressor {
public:
    struct TimeConstant { double attackMs, releaseMs; };
    static constexpr TimeConstant kTimeConstants[6]; // TC1–TC6
    struct Params {
        float inputGain=0.5, threshold=0.5, outputGain=0.5;
        int timeConstant=0; float mix=1;
    };
    void prepare(int numChannels, double sampleRate);
    void prepare(const patina::ProcessSpec&);
    void reset();
    float process(int ch, float x, const Params&);
    void processBlock(float*const*, int ch, int n, const Params&);
    float getGainReductionDb(int ch) const;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `inputGain` | 0.0–1.0 | Input gain |
| `threshold` | 0.0–1.0 | Compression threshold (0=-40dB, 1=0dB) |
| `outputGain` | 0.0–1.0 | Makeup gain |
| `timeConstant` | 0–5 | TC selector (attack 0.2–0.8ms, release 0.3–5s) |
| `mix` | 0.0–1.0 | Dry/Wet parallel compression |

---

## 47. VcaCompressor

`#include "dsp/circuits/dynamics/VcaCompressor.h"`

Classic large-format console / VCA compressor style.
THAT 2180 Blackmer VCA gain cell + diode rectifier + RC detector circuit.
Integrated as the `Vca` type in CompressorEngine.

**L2 Parts used:**
| Part | Instance | Circuit Role |
|------|------------|------------|
| `VcaPrimitive` | `vca` (THAT2180) | Gain cell — subtle nonlinearity from log/antilog BJT pair |
| `DiodePrimitive` | `scDiode` (Si1N4148) | Sidechain rectifier diode bridge |
| `RC_Element` | `detectorRC` (per ch) | Detector RC circuit — attack/release time constants controlled via R switching |

**Signal Flow:**
```
Audio ──────────────────────── VCA (THAT2180) ──→ Output
                                 ↑ gain
Audio → DiodeBridge (1N4148) → RC Detector (R_att/R_rel + C) → √ → dB → GR calculation
```

```cpp
class VcaCompressor {
public:
    struct Params {
        float threshold  = 0.5f;    // 0.0–1.0 → -40~0 dBFS
        float ratio      = 0.3f;    // 0.0–1.0 → 1.5:1~50:1
        float attack     = 0.3f;    // 0.0–1.0 → 0.1~80 ms
        float release    = 0.5f;    // 0.0–1.0 → 50~1200 ms
        float outputGain = 0.5f;    // Makeup gain 0.0–1.0
        float mix        = 1.0f;    // Dry/Wet 0.0–1.0
        int   kneeMode   = 0;       // 0=Soft knee, 1=Hard knee
    };
    void prepare(const patina::ProcessSpec&);
    void reset();
    float process(int ch, float x, const Params&);
    float getGainReductionDb(int ch = 0) const;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `threshold` | 0.0–1.0 | Compression threshold (0=-40dBFS, 1=0dBFS) |
| `ratio` | 0.0–1.0 | Compression ratio (0=1.5:1, 1=50:1) |
| `attack` | 0.0–1.0 | Attack (0=0.1ms, 1=80ms) — RC: R=100Ω~80kΩ, C=1μF |
| `release` | 0.0–1.0 | Release (0=50ms, 1=1200ms) — RC: R=50kΩ~1.2MΩ, C=1μF |
| `outputGain` | 0.0–1.0 | Makeup (0–12dB) |
| `kneeMode` | 0–1 | 0=Soft knee (6dB), 1=Hard knee |

---

## L3: Delay / Reverb

---

## 48. OversamplePath

`#include "dsp/circuits/delay/OversamplePath.h"`

Management utility for 2x/4x oversample buffers.

```cpp
static void ensureBuffer(AudioBuffer<float>& osBuf, int& osWritePos,
                         std::vector<float>& lastInputSample,
                         int numChannels, int desiredSamples);
static void write(AudioBuffer<float>& osBuf, int ch, int osWritePos, float value) noexcept;
static void advance(AudioBuffer<float>& osBuf, int& osWritePos) noexcept;
static float lerpSubsample(float prev, float now, int s, int osFactor) noexcept;
```
`lerpSubsample(prev, now, s, factor)` — returns the linearly interpolated value for subsample `s` (0-based).

---

## 49. SpringReverbModel

`#include "dsp/circuits/delay/SpringReverbModel.h"`

Classic spring reverb tank-style processor.
3-spring delay network (golden ratio spacing), 2-stage allpass diffuser, and drip effect.

```cpp
class SpringReverbModel {
public:
    struct Params {
        float decay      = 0.5f;    // 0.0–1.0
        float tone       = 0.5f;    // 0.0–1.0
        float mix        = 0.3f;    // 0.0–1.0 (dry/wet)
        float tension    = 0.5f;    // 0.0–1.0 (spring tension)
        float dripAmount = 0.3f;    // 0.0–1.0 (drip amount)
        int   numSprings = 3;       // 1–3
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(float* const* io, int numCh, int numSamples,
                       const Params& params) noexcept;
};
```

---

## 50. PlateReverb

`#include "dsp/circuits/delay/PlateReverb.h"`

Classic plate reverb unit-style processor.
4-line FDN (prime-number-spaced delays), 4-stage Schroeder allpass input diffuser,
Hadamard mixing, frequency-dependent damping, and internal modulation.

```cpp
class PlateReverb {
public:
    struct Params {
        float decay      = 0.5f;    // 0.0–1.0
        float predelayMs = 10.0f;   // 0–100 ms
        float damping    = 0.5f;    // 0.0–1.0 (HF damping)
        float mix        = 0.3f;    // 0.0–1.0 (dry/wet)
        float diffusion  = 0.7f;    // 0.0–1.0
        float modDepth   = 0.0f;    // 0.0–1.0 (internal mod depth)
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(float* const* io, int numCh, int numSamples,
                       const Params& params) noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `decay` | 0.0–1.0 | Reverb decay time |
| `predelayMs` | 0–100 ms | Predelay |
| `damping` | 0.0–1.0 | High-frequency damping (1.0 = dark) |
| `diffusion` | 0.0–1.0 | Input diffusion amount |
| `modDepth` | 0.0–1.0 | Chorus modulation inside FDN |

---

## L3: Saturation

---

## 51. TubePreamp

`#include "dsp/circuits/saturation/TubePreamp.h"`

12AX7 triode preamp emulation. Koren simplified plate curve model.
Grid conduction (even-order harmonics) and cathode bypass capacitor (low-frequency boost).

```cpp
class TubePreamp {
public:
    struct Params {
        float drive       = 0.5f;   // 0.0–1.0
        float bias        = 0.5f;   // 0.0–1.0
        float outputLevel = 0.7f;   // 0.0–1.0
        bool  enableGridConduction = true;
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(float* const* io, int numCh, int numSamples,
                       const Params& params) noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `drive` | 0.0–1.0 | Preamp gain |
| `bias` | 0.0–1.0 | Bias point (internal: `(bias-0.5)*1.6` → ±0.8V DC offset) |
| `outputLevel` | 0.0–1.0 | Output level |
| `enableGridConduction` | bool | Enable grid conduction (asymmetric distortion) |

---

## 52. TransformerModel

`#include "dsp/circuits/saturation/TransformerModel.h"`

British console-style audio transformer model.
Simplified Jiles-Atherton magnetic hysteresis, LF boost (~80 Hz), and HF rolloff (~18 kHz).

```cpp
class TransformerModel {
public:
    struct Params {
        float driveDb     = 0.0f;    // dB
        float saturation  = 0.5f;    // 0.0–1.0
        bool  enableLfBoost   = true;
        bool  enableHfRolloff = true;
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(float* const* io, int numCh, int numSamples,
                       const Params& params) noexcept;
};
```

---

## 53. TapeSaturation

`#include "dsp/circuits/saturation/TapeSaturation.h"`

Studio tape machine-style tape saturation.
Magnetic hysteresis, head bump (tape speed-dependent LF resonance), HF self-erase, and wow & flutter.

```cpp
class TapeSaturation {
public:
    struct Params {
        float inputGain     = 0.0f;   // dB
        float saturation    = 0.5f;   // 0.0–1.0
        float biasAmount    = 0.5f;   // 0.0–1.0
        float tapeSpeed     = 1.0f;   // 0.5–2.0 (1.0 = 15 ips)
        float wowFlutter    = 0.0f;   // 0.0–1.0
        bool  enableHeadBump  = true;
        bool  enableHfRolloff = true;
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(float* const* io, int numCh, int numSamples,
                       const Params& params) noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `inputGain` | dB | Tape input level |
| `saturation` | 0.0–1.0 | Magnetic saturation amount |
| `tapeSpeed` | 0.5–2.0 | Tape speed (affects head bump and HF character) |
| `wowFlutter` | 0.0–1.0 | Wow (0.5 Hz) + flutter (6 Hz) depth |

---

## 54. WaveFolder

`#include "dsp/circuits/saturation/WaveFolder.h"`

Classic West Coast synthesizer-style wavefolding circuit. Multi-stage cascade of DiodePrimitive nonlinear clipping and BJT_Primitive gain stages, generating harmonics through foldback distortion.

```cpp
class WaveFolder {
public:
    struct Params {
        float foldAmount   = 0.5f;    // Fold amount 0.0–1.0
        float symmetry     = 1.0f;    // Symmetry 0.0–1.0
        int   numStages    = 4;       // Fold stages 1–8
        float temperature  = 25.0f;   // Operating temperature (°C)
        int   diodeType    = 0;       // 0=Si, 1=Schottky, 2=Ge
    };

    void  prepare(int numChannels, double sampleRate) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(float* const* io, int numCh, int numSamples,
                       const Params& params) noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `foldAmount` | 0.0–1.0 | Fold intensity (0=clean, 1=maximum fold) |
| `symmetry` | 0.0–1.0 | Positive/negative symmetry (1.0=fully symmetric) |
| `numStages` | 1–8 | Fold stages (more stages = complex harmonic structure) |
| `diodeType` | 0–2 | 0=Si 1N4148, 1=Schottky 1N5818, 2=Ge OA91 |

---

## 55. PushPullPowerStage

`#include "dsp/circuits/saturation/PushPullPowerStage.h"`

Class AB push-pull tube power amplifier. Models a matched pentode pair, phase inverter, output transformer, negative feedback, power supply sag, and crossover distortion. Physical model of guitar/bass amp and hi-fi tube power stages.

**Parts used:** `PowerPentode` × 2 (matched pair) + `TransformerPrimitive` × 1 (output transformer)

**Signal flow:** Input → Phase Inverter → Tube A (push) + Tube B (pull) → Differential sum → Output Transformer → NFB loop → DC block → Output

```cpp
class PushPullPowerStage {
public:
    struct Params {
        float inputGainDb      = 0.0f;    // Input gain (dB)
        float bias             = 0.65f;   // 0=cold class B, 1=hot class A
        float piImbalance      = 0.02f;   // Phase inverter imbalance 0–0.2
        float tubeMismatch     = 0.03f;   // Tube pair mismatch 0–0.2
        float negativeFeedback = 0.3f;    // Global NFB 0–1.0
        float presenceHz       = 5000.0f; // NFB shelf corner frequency
        float sagAmount        = 0.5f;    // Power supply sag 0–1.0
        float outputLevel      = 0.7f;    // Output level 0–1.0
        float temperature      = 35.0f;   // Operating temperature (°C)
    };

    // Topology factory presets
    static PushPullPowerStage Marshall50W() noexcept;   // EL34, British OT
    static PushPullPowerStage FenderTwin() noexcept;    // 6L6GC, American OT
    static PushPullPowerStage VoxAC30() noexcept;       // EL84, hot bias
    static PushPullPowerStage HiFi_KT88() noexcept;     // KT88, ultralinear
    static PushPullPowerStage FenderDeluxe() noexcept;  // 6V6GT, compact OT

    PushPullPowerStage() noexcept;
    PushPullPowerStage(const PowerPentode::Spec& tubeA,
                       const PowerPentode::Spec& tubeB,
                       const TransformerPrimitive::Spec& xfmr) noexcept;
    void  prepare(int numChannels, double sampleRate) noexcept;
    void  prepare(const patina::ProcessSpec&) noexcept;
    void  reset() noexcept;
    float process(int ch, float x, const Params&) noexcept;
    void  processBlock(float*const*, int numCh, int numSamples,
                       const Params&) noexcept;
    const PowerPentode& getTubeA() const noexcept;
    const PowerPentode& getTubeB() const noexcept;
    const TransformerPrimitive& getOutputTransformer() const noexcept;
    double getAverageThermalState() const noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `inputGainDb` | −∞ – +∞ dB | Input gain to power stage |
| `bias` | 0.0–1.0 | Bias point (0=cold class B, 0.65=normal AB, 1.0=hot class A) |
| `piImbalance` | 0.0–0.2 | Phase inverter asymmetry |
| `tubeMismatch` | 0.0–0.2 | Matched pair imbalance (0=perfect match) |
| `negativeFeedback` | 0.0–1.0 | Global NFB amount (higher=cleaner, lower output Z) |
| `presenceHz` | 1k–10k Hz | Presence control: NFB rolloff frequency |
| `sagAmount` | 0.0–1.0 | Power supply sag (0=regulated, 1=vintage rectifier) |

**Topology presets:**

| Preset | Tubes | Transformer | NFB | Character |
|--------|-------|-------------|-----|-----------|
| `Marshall50W()` | EL34 × 2 | British Console | Moderate | Aggressive mids, crunch |
| `FenderTwin()` | 6L6GC × 2 | American Console | Heavy | Clean headroom, tight |
| `VoxAC30()` | EL84 × 2 | British Console | Low | Chimey, near class A |
| `HiFi_KT88()` | KT88 × 2 | API 2520 Output | High | Audiophile, low THD |
| `FenderDeluxe()` | 6V6GT × 2 | Compact FET Output | Moderate | Sweet breakup |

**Typical usage:**
```cpp
auto amp = PushPullPowerStage::Marshall50W();
amp.prepare(2, 48000.0);

PushPullPowerStage::Params p;
p.inputGainDb = 12.0f;
p.bias = 0.65f;
p.negativeFeedback = 0.3f;
p.sagAmount = 0.6f;
float out = amp.process(0, preampOutput, p);
```

---

## L3: Modulation

---

## 56. AnalogLfo

`#include "dsp/circuits/modulation/AnalogLfo.h"`

Triangle-wave LFO generated by a TL072 op-amp integrator + Schmitt trigger. Also provides a sine-like output with rounded corners. Approximately 90° phase offset between stereo channels.

### Methods

```cpp
void prepare(int numChannels, double sampleRate);
void prepare(const patina::ProcessSpec& spec);
void reset();
```

```cpp
void setRateHz(double hz);
```
LFO rate (0.05–20 Hz).

```cpp
void setSupplyVoltage(double v);
```
Supply voltage 5–36 V. Affects Schmitt threshold and bandwidth.

```cpp
inline void stepAll() noexcept;
```
Advances all channels by 1 sample. **Call per-sample on the audio thread.**

```cpp
inline float getTri(int channel) const noexcept;   // Triangle [-1, 1]
inline float getSinLike(int channel) const noexcept; // Sine-like [-1, 1]
```

```cpp
void ensureChannels(int numChannels);
```
Re-`prepare` if channel count changes.

---

## 57. ModulationBus

`#include "dsp/circuits/modulation/ModulationBus.h"`

Utility that distributes AnalogLfo output as stereo modulation signals.

```cpp
static double stereoSign(int ch) noexcept;
```
Returns -0.75 for odd channels (partial phase inversion).

```cpp
static double tri(const AnalogLfo& lfo, int ch, float depth01) noexcept;
```
Returns triangle-wave LFO value with stereo sign.

```cpp
static double sinv(const AnalogLfo& lfo, int ch, bool dual,
                   double common, float depth01) noexcept;
static double sinvForClock(const AnalogLfo& lfo, int ch, bool dual,
                            double common, float depth01) noexcept;
```
`sinvForClock` blends 82% sine + 18% triangle, reproducing the phase offset used for clock modulation.

```cpp
static double commonSinv(const AnalogLfo& lfo) noexcept;
```
Returns a common sine-like value from ch0 (for dual mode).

---

## 58. StereoImage

`#include "dsp/circuits/modulation/StereoImage.h"`

Stereo widening via Mid/Side matrix.

```cpp
static void widenEqualPowerSIMD(float& L, float& R, float depth01) noexcept;
```
`depth01` 0.0–1.0 → scales the Side component by 0.6×–0.85×.

```cpp
static void widenEqualPower(std::vector<float>& stereoVolt, float depth01) noexcept;
```
`stereoVolt[0]=L, stereoVolt[1]=R` vector version.

---

## 59. RingModulator

`#include "dsp/circuits/modulation/RingModulator.h"`

Diode-bridge ring modulator. Constructs a bridge circuit from 4 DiodePrimitive instances, functioning as a 2-input multiplier. Reproduces diode mismatch, temperature dependence, and nonlinear distortion.

```cpp
class RingModulator {
public:
    struct Params {
        float mix          = 1.0f;    // Dry/Wet 0.0–1.0
        float temperature  = 25.0f;   // Operating temperature (°C)
        int   diodeType    = 0;       // 0=Si, 1=Schottky, 2=Ge
        float mismatch     = 0.02f;   // Diode mismatch 0.0–0.1
    };

    void  prepare(int numChannels, double sampleRate) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float input, float carrier,
                  const Params& params) noexcept;
    void  processBlock(float* const* io, const float* const* carrier,
                       int numCh, int numSamples, const Params& params) noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `mix` | 0.0–1.0 | Dry/Wet balance |
| `diodeType` | 0–2 | 0=Si 1N4148, 1=Schottky 1N5818, 2=Ge OA91 |
| `mismatch` | 0.0–0.1 | Manufacturing variation between 4 diodes (affects carrier leak) |

---

## L3: Mid/Side

---

## 60. MidSideIron

`#include "dsp/circuits/modulation/MidSideIron.h"`

Passive transformer-based M/S matrix. Physically models two transformer windings that additively and differentially combine signals, imparting the magnetic saturation, LF warmth, and HF character inherent to transformer iron — the source of the "iron" sound in console M/S processing.

**L2 Parts used:** `TransformerPrimitive × 4`
- `xfmrEnc[0]` — encode Mid path
- `xfmrEnc[1]` — encode Side path
- `xfmrDec[0]` — decode L path
- `xfmrDec[1]` — decode R path
- Default preset: `MidSideConsole()` — Neve-style 1:1, moderate core saturation (600Ω, Lp=10H, k=0.995, coreSatLevel=1.2)

```cpp
class MidSideIron {
public:
    struct Params {
        float satAmount   = 0.15f;  // Transformer core saturation drive (0.0–1.0)
        float temperature = 25.0f;  // Operating temperature (°C) — affects core permeability
    };

    static constexpr TransformerPrimitive::Spec MidSideConsole() noexcept;  // Built-in preset

    MidSideIron() noexcept;
    explicit MidSideIron(const TransformerPrimitive::Spec& spec) noexcept;

    void prepare(double sampleRate) noexcept;
    void prepare(const patina::ProcessSpec& spec) noexcept;
    void reset() noexcept;

    struct MidSide { float mid; float side; };
    struct Stereo  { float left; float right; };

    MidSide encode(float left, float right, const Params& p = {}) noexcept;
    Stereo  decode(float mid,  float side,  const Params& p = {}) noexcept;
    Stereo  process(float left, float right,
                    float midGain, float sideGain,
                    const Params& p = {}) noexcept;

    const TransformerPrimitive& getEncodeTransformer(int index) const noexcept;  // index 0 or 1
    const TransformerPrimitive& getDecodeTransformer(int index) const noexcept;  // index 0 or 1
};
```

**Encode (L/R → M/S):**
```
Mid  = Xfmr( (L+R) × 0.5 )   // sum through transformer
Side = Xfmr( (L−R) × 0.5 )   // diff through transformer
```

**Decode (M/S → L/R):**
```
L = Xfmr( Mid + Side )
R = Xfmr( Mid − Side )
```

| Parameter | Range | Description |
|-----------|-------|-------------|
| `satAmount` | 0.0–1.0 | Core saturation drive. 0=clean, 0.15=subtle warmth (default), 1.0=heavy iron |
| `temperature` | 0–80°C | Core permeability temperature dependence (μ(T) = μ₀ × (1 + k_temp × ΔT)) |

**Usage:**
```cpp
MidSideIron msIron;
msIron.prepare(spec);

// Encode → process independently → decode
for (int i = 0; i < numSamples; ++i) {
    auto [mid, side] = msIron.encode(L[i], R[i]);
    mid  *= midGain;
    side *= sideGain;
    auto [outL, outR] = msIron.decode(mid, side);
    L[i] = outL; R[i] = outR;
}

// Or single-call round-trip
for (int i = 0; i < numSamples; ++i) {
    auto [outL, outR] = msIron.process(L[i], R[i], midGain, sideGain);
    L[i] = outL; R[i] = outR;
}
```

**Custom transformer spec:**
```cpp
// Use British console character instead of default
MidSideIron msIron(TransformerPrimitive::BritishConsole());
```

---

## 61. MidSidePrecision

`#include "dsp/circuits/modulation/MidSidePrecision.h"`

Active op-amp-based M/S matrix. Uses op-amp inverting amplifiers to perform summation and subtraction electrically. Compared to the transformer method, this is extremely clean and accurate — the method adopted by high-end mastering consoles for transparent M/S processing.

**L2 Parts used:** `OpAmpPrimitive × 4`
- `opampEnc[0]` — encode Mid path
- `opampEnc[1]` — encode Side path
- `opampDec[0]` — decode L path
- `opampDec[1]` — decode R path
- Default IC: `TL072CP` (JFET input, 13V/µs slew, 3MHz GBW)

```cpp
class MidSidePrecision {
public:
    struct Params {
        bool highVoltage = true;  // true=18V supply, false=9V supply
    };                            // Affects saturation threshold and headroom

    MidSidePrecision() noexcept;
    explicit MidSidePrecision(const OpAmpPrimitive::Spec& spec) noexcept;

    void prepare(double sampleRate) noexcept;
    void prepare(const patina::ProcessSpec& spec) noexcept;
    void reset() noexcept;

    struct MidSide { float mid; float side; };
    struct Stereo  { float left; float right; };

    MidSide encode(float left, float right, const Params& p = {}) noexcept;
    Stereo  decode(float mid,  float side,  const Params& p = {}) noexcept;
    Stereo  process(float left, float right,
                    float midGain, float sideGain,
                    const Params& p = {}) noexcept;

    const OpAmpPrimitive& getEncodeOpAmp(int index) const noexcept;  // index 0 or 1
    const OpAmpPrimitive& getDecodeOpAmp(int index) const noexcept;  // index 0 or 1
};
```

**Encode (L/R → M/S):**
```
Mid  = OpAmp( (L+R) × 0.5 )   // summing amp + slew/saturation
Side = OpAmp( (L−R) × 0.5 )   // difference amp + slew/saturation
```

**Decode (M/S → L/R):**
```
L = OpAmp( Mid + Side )
R = OpAmp( Mid − Side )
```

| Parameter | Range | Description |
|-----------|-------|-------------|
| `highVoltage` | bool | `true`=18V rail (satThreshold=0.88, default), `false`=9V (satThreshold=0.75, more saturation) |

**IC Presets:**
| Preset | Slew | GBW | Character |
|--------|------|-----|-----------|
| `TL072CP()` | 13 V/µs | 3 MHz | Bright, open, JFET (default) |
| `NE5532()` | 9 V/µs | 10 MHz | Dense, punchy console character |
| `OPA2134()` | 20 V/µs | 8 MHz | Hi-fi clean, high headroom |
| `LM4562()` | 20 V/µs | 55 MHz | Ultra-transparent mastering grade |
| `JRC4558D()` | 0.5 V/µs | 1 MHz | Warm/dark vintage character |

**Usage:**
```cpp
// Transparent mastering-grade processing with LM4562
MidSidePrecision msPrec(OpAmpPrimitive::LM4562());
msPrec.prepare(spec);

for (int i = 0; i < numSamples; ++i) {
    auto [mid, side] = msPrec.encode(L[i], R[i]);
    mid  *= midGain;
    side *= sideGain;
    auto [outL, outR] = msPrec.decode(mid, side);
    L[i] = outL; R[i] = outR;
}
```

---

## L3: Mixer

---

## 62. Mixer

`#include "dsp/circuits/mixer/Mixer.h"`

Dry/Wet equal-power crossfade, ducking, and attack-detection ducking.

### Key Methods

```cpp
static void equalPowerGainsFast(double mix01, float& gDry, float& gWet) noexcept;
```
`mix01` 0.0–1.0 → calculates `gDry`/`gWet` with equal-power ratio (uses LUT).

```cpp
static void equalPowerGains(double mix01, float& gDry, float& gWet) noexcept;
```
Same as above, floating-point version (backward compatible).

---

---

## 63. DuckingMixer

`#include "dsp/circuits/mixer/DuckingMixer.h"`

Extends `DryWetMixer` (equal-power crossfade) with analog-style ducking: when the wet mix is high, the dry signal's energy momentarily ducks the wet gain to prevent pumping on loud transients. Also provides attack-detection ducking via a high-pass envelope follower.

Used internally by `BbdDelayEngine` to replicate the level interaction characteristic of classic analog delay pedals.

### Key Constants

| Constant | Value | Description |
|---|---|---|
| `kDuckingAmount` | 0.01 | Ducking depth coefficient |
| `kDuckingMinFloor` | 0.6 | Minimum wet gain floor |
| `kAttackDuckingAmount` | 0.05 | Extra ducking on transient attack |
| `kAttackEnvAttackTime` | 1 ms | Attack envelope attack time |
| `kAttackEnvReleaseTime` | 50 ms | Attack envelope release time |

### Key Method

```cpp
static float analogDuckingMix(float dryV, float wetV, double mix01,
                              float wetMakeupBase = kWetMakeupGain,
                              float duckingAmount = kDuckingAmount) noexcept;
```

---

## 64. GainUtils

`#include "dsp/circuits/mixer/GainUtils.h"`

Stateless helper struct that chains `DuckingMixer` with level compensation, output-stage saturation, and full-scale conversion into a single `mixToFs()` call. Used as the final output stage inside `BbdDelayEngine`.

### Key Methods

```cpp
// Instrument (+20 dB) or line (+12 dB) level compensation
static float applyLevelComp(float v, bool isInstrument) noexcept;

// Apply master gain and optional OutputStage op-amp saturation
static float applyMasterAndSaturation(float v, int channel, double masterGain,
                                      OutputStage& outMod,
                                      bool emulateOpAmpSaturation,
                                      double effectiveSupplyV) noexcept;

// Convert voltage-domain sample to full-scale float
static float toFS(float v, float vToFsGain) noexcept;

// Full pipeline: ducking mix → level comp → saturation → FS conversion
static float mixToFs(int channel, float dryV, float wetV,
                     double mix01, double masterGain, bool isInstrument,
                     OutputStage& outMod, bool emulateOpAmpSaturation,
                     double effectiveSupplyV, float vToFsGain,
                     bool enableAnalogDucking = true) noexcept;
```

---

## L3: Power

---

## 65. PowerSupplySag

`#include "dsp/circuits/power/PowerSupplySag.h"`

Classic tube rectifier (GZ34) power supply sag model.
Load-current-dependent B+ voltage droop, envelope follower for current demand tracking,
filter capacitor charge/discharge, ripple injection, and temperature-dependent rectifier resistance.

```cpp
class PowerSupplySag {
public:
    struct Params {
        float rectifierResistance = 25.0f;   // Rectifier resistance (Ω)
        float filterCapUf         = 50.0f;   // Filter capacitor (µF)
        float idleCurrentMa       = 80.0f;   // Idle current (mA)
        float maxCurrentMa        = 250.0f;  // Max load current (mA)
        float attackMs            = 2.0f;    // Attack time (ms)
        float releaseMs           = 80.0f;   // Release time (ms)
        float sagDepth            = 1.0f;    // Sag depth (0.0–1.0)
        float rippleHz            = 100.0f;  // Ripple frequency (Hz)
        float rippleDepth         = 0.002f;  // Ripple depth
        float temperature         = 25.0f;   // Ambient temperature (°C)
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(const float* const* input, float* const* output,
                       float* sagOut, int numCh, int numSamples,
                       const Params& params) noexcept;

    float getSagLevel(int channel) const noexcept;

    // Helper: sag coefficient → pedal supply voltage (9V–18V)
    static double sagToSupplyVoltage(float sagLevel) noexcept;
    // Helper: sag coefficient → tube amp B+ voltage
    static double sagToBPlusVoltage(float sagLevel, double nominalBPlus = 450.0) noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `rectifierResistance` | 10–100 Ω | Rectifier tube internal resistance (GZ34≈25, 5Y3≈60, 5U4≈50) |
| `filterCapUf` | 10–200 µF | Filter capacitor value |
| `idleCurrentMa` | 10–200 mA | Idle current consumption |
| `maxCurrentMa` | 50–500 mA | Maximum current at full power |
| `sagDepth` | 0.0–1.0 | Sag effect depth (0=bypass) |
| `rippleHz` | 10–500 Hz | Power supply ripple frequency (100Hz=50Hz full-wave rectification) |
| `rippleDepth` | 0.0–0.05 | Ripple amount (amplified under load) |
| `temperature` | -20–60 °C | Temperature dependence of rectifier resistance |

**Output**: `process()` returns the supply voltage coefficient (0.0–1.0). 1.0 = rated voltage, lower = sag.  
**Connection example**: Convert to 9V–18V pedal voltage via `sagToSupplyVoltage(sag)` and feed to `supplyVoltage` on InputBuffer/OutputStage.

---

## 66. BatterySag

`#include "dsp/circuits/power/BatterySag.h"`

9V battery dying-battery effect. Models voltage drop and internal resistance increase from battery
depletion in classic fuzz / treble booster / overdrive-style pedals.
Reproduces discharge characteristics of 3 battery types (Alkaline / CarbonZinc / Rechargeable).

```cpp
class BatterySag {
public:
    enum BatteryType : int { Alkaline=0, CarbonZinc=1, Rechargeable=2 };

    struct BatterySpec {
        const char* name;
        double freshVoltage, deadVoltage, freshResistance, dischargeCurve;
    };
    static constexpr BatterySpec batterySpecs[3];

    struct Params {
        int   batteryType   = Alkaline;
        float batteryLife   = 1.0f;      // 0.0=empty, 1.0=full charge
        float loadCurrentMa = 10.0f;     // Circuit current draw (mA)
        float sagAmount     = 1.0f;      // Dynamic sag amount (0–1)
        float attackMs      = 5.0f;
        float releaseMs     = 50.0f;
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(const float* const* input, float* const* output,
                       float* voltageOut, int numCh, int numSamples,
                       const Params& params) noexcept;

    // Static utilities
    static double getOpenCircuitVoltage(int type, float life) noexcept;
    static double getInternalResistance(int type, float life) noexcept;
    static float  voltageToSagLevel(float voltage) noexcept;
    float getVoltage(int type, float life, float loadCurrentMa) const noexcept;
};
```

| Battery Type | Fresh Voltage | Dead Voltage | Internal Resistance | Character |
|---|---|---|---|---|
| Alkaline | 9.6V | 5.5V | 3Ω | Gradual decline, steep drop at end |
| CarbonZinc | 9.0V | 5.0V | 10Ω | Vintage, gradual discharge |
| Rechargeable | 8.4V | 6.0V | 1Ω | Flat plateau → steep drop |

| Parameter | Range | Description |
|-----------|------|------|
| `batteryLife` | 0.0–1.0 | Battery remaining (0=empty) |
| `loadCurrentMa` | 1–50 mA | Circuit current consumption |
| `sagAmount` | 0.0–1.0 | Dynamic sag depth (0=bypass) |

**Output**: `process()` returns effective voltage (V). Normalizable to 0–1 via `voltageToSagLevel()`.  
**Choosing PowerSupplySag vs. BatterySag**: Use BatterySag for pedal applications, PowerSupplySag for tube amplifiers.

---

## 67. AdapterSag

`#include "dsp/circuits/power/AdapterSag.h"`

DC adapter power supply sag model. Emulates supply characteristics of 9V / 18V pedal DC adapters.
3 regulation types × 2 voltages (6 types): linear, switching, and unregulated.

```cpp
class AdapterSag {
public:
    enum AdapterType : int {
        Linear9V=0, Linear18V, Switching9V, Switching18V,
        Unregulated9V, Unregulated18V
    };

    struct AdapterSpec {
        const char* name;
        double nominalVoltage, outputResistance, rippleFreqHz;
        double rippleAmplitude, maxCurrentMa, currentLimitKnee;
        bool isRegulated;
    };
    static constexpr AdapterSpec adapterSpecs[6];

    struct Params {
        int   adapterType    = Linear9V;
        float loadCurrentMa  = 20.0f;
        float sagAmount      = 1.0f;      // Dynamic sag amount
        float attackMs       = 3.0f;
        float releaseMs      = 30.0f;
        float rippleMix      = 1.0f;      // Ripple injection amount
        float mainsHz        = 50.0f;     // Mains frequency
    };

    void  prepare(int numChannels, double sr) noexcept;
    void  prepare(const patina::ProcessSpec& spec) noexcept;
    void  reset() noexcept;
    float process(int channel, float x, const Params& params) noexcept;
    void  processBlock(const float* const* in, float* const* out,
                       float* voltageOut, int numCh, int numSamples,
                       const Params& params) noexcept;

    static double getStaticVoltage(int type, float loadCurrentMa) noexcept;
    static double getNominalVoltage(int type) noexcept;
    static float  voltageToSagLevel(float voltage, int type) noexcept;
};
```

| Adapter Type | Nominal Voltage | Output Impedance | Ripple | Regulated |
|---|---|---|---|---|
| Linear9V | 9.0V | 0.5Ω | 100Hz / 10mV | Yes |
| Linear18V | 18.0V | 0.8Ω | 100Hz / 15mV | Yes |
| Switching9V | 9.0V | 0.2Ω | 65kHz / 30mV | Yes |
| Switching18V | 18.0V | 0.3Ω | 65kHz / 40mV | Yes |
| Unregulated9V | 9.6V | 5.0Ω | 100Hz / 200mV | No |
| Unregulated18V | 19.2V | 8.0Ω | 100Hz / 350mV | No |

| Parameter | Range | Description |
|-----------|------|------|
| `loadCurrentMa` | 1–500 mA | Circuit current consumption |
| `sagAmount` | 0.0–1.0 | Dynamic sag depth |
| `rippleMix` | 0.0–1.0 | Ripple injection amount |
| `mainsHz` | 50/60 Hz | Mains frequency (affects linear ripple) |

**Choosing BatterySag vs. AdapterSag**: Use BatterySag for battery power (dying battery), AdapterSag for DC adapters.  
**18V use case**: Use Linear18V / Switching18V to model modern high-headroom pedals.

---

## 68. CapacitorAging

`#include "dsp/circuits/power/CapacitorAging.h"`

Capacitor aging simulation. Models capacitance decrease and ESR increase over time,
dynamically modulating `ModdingConfig::CapGradeSpec`. Features Arrhenius temperature acceleration model.

```cpp
class CapacitorAging {
public:
    struct Params {
        float ageYears       = 0.0f;    // Service years (0=new, 40=vintage)
        float temperature    = 25.0f;   // Ambient temperature (°C)
        float qualityFactor  = 1.0f;    // Quality factor (0.5–2.0)
        float humidityFactor = 1.0f;    // Humidity acceleration factor
    };

    void setParams(const Params& p) noexcept;
    void setAge(float years) noexcept;
    void setTemperature(float tempC) noexcept;
    void setQualityFactor(float q) noexcept;
    void setHumidityFactor(float h) noexcept;

    // Post-degradation scale values
    double getCapacitanceScale() const noexcept;                           // Remaining capacitance ratio
    double getCapacitanceScale(const ModdingConfig::CapGradeSpec&) const;  // CapGrade-aware
    double getEsrScale() const noexcept;                                   // ESR increase ratio
    double getEsrScale(const ModdingConfig::CapGradeSpec&) const;          // CapGrade-aware
    double getMicrophonicsScale() const noexcept;
    double getMicrophonicsScale(const ModdingConfig::CapGradeSpec&) const;

    // Generate post-degradation CapGradeSpec
    ModdingConfig::CapGradeSpec getAgedSpec(
        const ModdingConfig::CapGradeSpec& base) const noexcept;

    double getEffectiveAge() const noexcept;

    struct AgingSummary {
        double effectiveAge, capacitanceScale, esrScale, microphonicsScale;
    };
    AgingSummary getSummary() const noexcept;
    AgingSummary getSummary(const ModdingConfig::CapGradeSpec&) const noexcept;
};
```

| Parameter | Range | Description |
|-----------|------|------|
| `ageYears` | 0–40+ years | Capacitor service years (0=new) |
| `temperature` | -20–60 °C | Effective age acceleration (doubles per 10°C rise) |
| `qualityFactor` | 0.1–2.0 | Quality (higher = slower degradation) |
| `humidityFactor` | 0.5–2.0 | Accelerated degradation from humidity |

**Degradation model**:
- Capacitance: `C(t) = C₀ × (1 - k × age_eff)` — minimum 50%
- ESR: `ESR(t) = ESR₀ × (1 + m × age_eff²)` — maximum 5×
- Temperature acceleration: `age_eff = age × 2^((T - 25) / 10)` (Arrhenius approximation)

**Connection example**: `getCapacitanceScale()` → `BbdDelayEngine::Params::capacitanceScale`, `getAgedSpec()` → dynamic selection of ModdingConfig CapGrade.

---

# L4 — Engines

> Top-level layer integrating L3 circuit modules. Common API: `prepare()` → `processBlock()` → `reset()`.
> Application development can be done using this layer alone.
>
> **pedalMode (common to all engines):**
> All L4 engine Params include `bool pedalMode = false`.
> - `false` (default): **Outboard quality** — bypasses InputBuffer / OutputStage for transparent, high-quality processing
> - `true`: **Pedal quality** — enables InputBuffer (TL072 RC LPF + slew-rate limiting + headroom saturation) and
>   OutputStage (3-pole cascade LPF + soft saturation), reproducing the coloration unique to pedal circuits

---

## 69. BbdDelayEngine

`#include "dsp/engine/BbdDelayEngine.h"`

High-level engine that integrates the full chain of a BBD analog delay into a single class.

> Internal signal path:  
> Outboard: Input → Compander(compress) → DelayLine → BbdSampler(S&H) → BbdStageEmulator  
> → Compander(expand) → ToneFilter → Mixer(dry/wet)  
> Pedal: Input → **InputBuffer** → **InputFilter** → Compander(compress)  
> → DelayLine → BbdSampler → BbdStageEmulator  
> → Compander(expand) → ToneFilter → **OutputStage** → Mixer(dry/wet)  
> (BbdFeedback → returns to delay write point)

### `struct Params`
```cpp
struct Params {
    float delayMs       = 250.0f;   // Delay time (ms)
    float feedback      = 0.3f;     // Feedback amount 0.0–1.0
    float tone          = 0.5f;     // Tone 0.0–1.0
    float mix           = 0.5f;     // Dry/Wet mix 0.0–1.0
    float compAmount    = 0.5f;     // Compander amount 0.0–1.0
    float chorusDepth   = 0.0f;     // Chorus depth 0.0–1.0
    float lfoRateHz     = 0.5f;     // LFO rate (Hz)
    double supplyVoltage = 9.0;     // Supply voltage (V)
    int    bbdStages     = 8192;    // BBD stage count
    bool emulateBBD        = true;
    bool emulateOpAmpSat   = true;
    bool emulateToneRC     = true;
    bool enableAging       = false;
    double ageYears        = 0.0;
    double capacitanceScale = 1.0;
};
```

### Methods

```cpp
void prepare(const patina::ProcessSpec& spec);
void reset();
```
Initializes/resets all internal modules at once. Allocates delay buffer (max 2 seconds).

```cpp
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params);
```
Main block processing. `input == output` (in-place) is allowed.

```cpp
void applyModdingConfig(const ModdingConfig& mod);
```
Applies op-amp, compander, and capacitor grade settings from `ModdingConfig` in one call.

```cpp
void setCompressorConfig(const CompanderModule::Config& cfg);
void setExpanderConfig(const CompanderModule::Config& cfg);
void setOpAmpOverrides(const OpAmpPrimitive::Spec& ovr);  // = BbdFeedback::OpAmpOverrides
void setBandwidthScale(double s);
void setOutputCutoffHz(double fc);
```
Detailed settings for individual modules.

```cpp
const InputBuffer&   getInputBuffer()   const;
const InputFilter&           getInputFilter()      const;
const CompanderModule&    getCompressor()    const;
const CompanderModule&    getExpander()      const;
const BbdStageEmulator&   getBbdStageEmu()   const;
const ToneFilter&     getToneFilter()    const;
const BbdFeedback&     getFeedbackMod()   const;
const OutputStage&       getOutputMod()     const;
```
Read-only access to internal modules (for testing/diagnostics).

**Typical usage:**
```cpp
patina::BbdDelayEngine engine;
patina::ProcessSpec spec{48000.0, 256, 2};
engine.prepare(spec);

// MOD settings (optional)
ModdingConfig mod;
mod.opAmp = ModdingConfig::JRC4558D;
mod.compander = ModdingConfig::NE570;
mod.capGrade = ModdingConfig::Film;
engine.applyModdingConfig(mod);

// Block processing
patina::BbdDelayEngine::Params params;
params.delayMs   = 300.0f;
params.feedback  = 0.5f;
params.tone      = 0.6f;
params.mix       = 0.4f;
params.compAmount = 0.7f;
engine.processBlock(input, output, 2, blockSize, params);
```

---

## 70. DriveEngine

`#include "dsp/engine/DriveEngine.h"`

Pedal-style drive/overdrive integrated engine.

> Signal path:  
> Outboard: Input → DiodeClipper → ToneFilter → Dry/Wet  
> Pedal: Input → **InputBuffer(TL072)** → DiodeClipper → ToneFilter → **OutputStage** → Dry/Wet  
> (Reproduces dynamic headroom fluctuation via PowerSupplySag)

### `struct Params`
```cpp
struct Params {
    float drive         = 0.5f;     // Drive amount 0.0–1.0
    int   clippingMode  = 1;        // 0=Bypass, 1=Diode, 2=Tanh
    int   diodeType     = 0;        // 0=Si, 1=Schottky, 2=Ge
    float tone          = 0.5f;     // Tone 0.0–1.0
    float outputLevel   = 0.7f;     // Output level 0.0–1.0
    float mix           = 1.0f;     // Dry/Wet 0.0–1.0
    double supplyVoltage = 9.0;     // Supply voltage (V)
    float temperature   = 25.0f;    // Operating temperature (°C)
    bool  enablePowerSag = false;   // Enable power sag
    float sagAmount      = 0.5f;    // Sag amount 0.0–1.0

    bool  pedalMode      = false;   // true=pedal quality, false=outboard quality
};
```

### Methods

```cpp
void prepare(const patina::ProcessSpec& spec);
void reset();
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params);
```

**Typical usage:**
```cpp
patina::DriveEngine drive;
drive.prepare({48000.0, 512, 2});

patina::DriveEngine::Params p;
p.drive = 0.7f;
p.clippingMode = 1;  // Diode
p.diodeType = 2;     // Ge
p.tone = 0.6f;
p.enablePowerSag = true;
drive.processBlock(input, output, 2, blockSize, p);
```

---

## 71. ReverbEngine

`#include "dsp/engine/ReverbEngine.h"`

Spring/Plate reverb integrated engine. Switch between two reverb algorithms by type.

> Signal path:  
> Outboard: Input → [SpringReverb | PlateReverb] → ToneFilter → Dry/Wet  
> Pedal: Input → **InputBuffer** → [SpringReverb | PlateReverb] → ToneFilter → **OutputStage** → Dry/Wet

### `enum Type`
| Value | Description |
|---|---|
| `Spring` | Classic spring reverb (3 springs in parallel, drip effect) |
| `Plate` | Classic plate reverb (4-line FDN, Schroeder diffuser) |

### `struct Params`
```cpp
struct Params {
    int   type           = Spring;  // Reverb type
    float decay          = 0.5f;    // Decay 0.0–1.0
    float tone           = 0.5f;    // Tone 0.0–1.0
    float mix            = 0.3f;    // Dry/Wet 0.0–1.0
    double supplyVoltage = 9.0;     // Supply voltage (V)
    // Spring-specific
    float tension        = 0.5f;    // Spring tension 0.0–1.0
    float dripAmount     = 0.3f;    // Drip effect amount 0.0–1.0
    int   numSprings     = 3;       // Number of springs (1–3)
    // Plate-specific
    float predelayMs     = 10.0f;   // Predelay (ms)
    float damping        = 0.5f;    // Damping 0.0–1.0
    float diffusion      = 0.7f;    // Diffusion 0.0–1.0
    float modDepth       = 0.0f;    // Internal mod depth 0.0–1.0

    bool  pedalMode      = false;   // true=pedal quality, false=outboard quality
};
```

### Methods

```cpp
void prepare(const patina::ProcessSpec& spec);
void reset();
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params);
```

**Typical usage:**
```cpp
patina::ReverbEngine reverb;
reverb.prepare({48000.0, 512, 2});

patina::ReverbEngine::Params p;
p.type = patina::ReverbEngine::Plate;
p.decay = 0.7f;
p.damping = 0.4f;
p.predelayMs = 20.0f;
p.mix = 0.3f;
reverb.processBlock(input, output, 2, blockSize, p);
```

---

## 72. CompressorEngine

`#include "dsp/engine/CompressorEngine.h"`

Photo / FET / Variable-Mu / VCA switchable dynamics integrated engine.

> Signal path:  
> Outboard: Input → [NoiseGate(opt)] → [Photo | FET | VariableMu | VCA] → Dry/Wet  
> Pedal: Input → **InputBuffer** → [NoiseGate(opt)] → [Photo | FET | VariableMu | VCA] → **OutputStage** → Dry/Wet

### `enum Type`
| Value | Description |
|---|---|
| `Photo` | T4B photocoupler compressor (program-dependent attack/release) |
| `Fet` | 2N5457 FET compressor (ultra-fast attack 20–800μs) |
| `VariableMu` | 6386 variable-mu tube compressor (push-pull circuit) |
| `Vca` | Clean VCA compressor (large-format console / classic dynamics processor style, RMS detection) |

### `struct Params`
```cpp
struct Params {
    int   type            = Fet;        // Compressor type
    float inputGain       = 0.5f;       // Input gain 0.0–1.0
    float threshold       = 0.5f;       // Threshold 0.0–1.0 (all models: higher = less compression)
                                        //   Photo: internally maps 1-threshold → peakReduction
                                        //   VariableMu: 0=-40dB, 1=0dB
                                        //   FET: unused (controlled by inputGain)
    float outputGain      = 0.5f;       // Makeup gain 0.0–1.0
    float attack          = 0.5f;       // Attack 0.0–1.0 (FET/VCA)
    float release         = 0.5f;       // Release 0.0–1.0 (FET/VCA)
    int   ratio           = 0;          // FET: 0–4 (4:1–All), VariableMu: TC 0–5, VCA: 0–4
    float mix             = 1.0f;       // Dry/Wet 0.0–1.0
    double supplyVoltage  = 9.0;
    bool  enableGate      = false;      // Pre-stage noise gate
    float gateThresholdDb = -40.0f;     // Gate threshold (dBFS)
    int   photoMode       = 0;          // 0=Compress, 1=Limit
    int   vcaKneeMode     = 0;          // 0=Soft knee, 1=Hard knee

    bool  pedalMode       = false;      // true=pedal quality, false=outboard quality
};
```

### Methods

```cpp
void prepare(const patina::ProcessSpec& spec);
void reset();
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params);

// Gain reduction readout
float getPhotoGainReductionDb(int channel = 0) const;
float getFetGainReductionDb(int channel = 0) const;
float getVarMuGainReductionDb(int channel = 0) const;
float getVcaGainReductionDb(int channel = 0) const;
bool  isGateOpen(int channel = 0) const;
```

**Typical usage:**
```cpp
patina::CompressorEngine comp;
comp.prepare({48000.0, 512, 2});

patina::CompressorEngine::Params p;
p.type = patina::CompressorEngine::Fet;
p.inputGain = 0.7f;
p.ratio = 4;       // All-Buttons
p.enableGate = true;
comp.processBlock(input, output, 2, blockSize, p);
float gr = comp.getFetGainReductionDb(0);
```

---

## 73. EnvelopeGeneratorEngine

`#include "dsp/engine/EnvelopeGeneratorEngine.h"`

ADSR / AD / AR envelope generator engine. Integrates `EnvelopeGenerator` (RC timing circuit built from `DiodePrimitive`, `OTA_Primitive`, `BJT_Primitive`, `RC_Element`) with a `VcaPrimitive` (THAT2180) gain cell to shape audio amplitude.

### Signal Path

```
Outboard: Input → VCA(ADSR envelope) → OutputGain → Dry/Wet
Pedal:    Input → InputBuffer → VCA(ADSR envelope) → OutputGain → OutputStage → Dry/Wet
```

### `enum TriggerMode`

| Value | Description |
|---|---|
| `External` | Gate via `gateOn()` / `gateOff()` (synth / sequencer use) |
| `Auto` | Auto-trigger when input exceeds `autoThresholdDb` (effect use) |

### `struct Params`

```cpp
struct Params {
    float attack    = 0.3f;   // 0.0–1.0 → 0.5 ms – 5000 ms
    float decay     = 0.3f;   // 0.0–1.0 → 5 ms – 5000 ms
    float sustain   = 0.7f;   // 0.0–1.0
    float release   = 0.4f;   // 0.0–1.0 → 10 ms – 10000 ms
    int   envMode   = 0;      // 0=ADSR, 1=AD, 2=AR
    int   curve     = 0;      // 0=RC exponential, 1=Linear
    int   triggerMode     = External;
    float autoThresholdDb = -30.0f;  // Auto-trigger threshold (dBFS)
    float outputGain = 1.0f;
    float mix        = 1.0f;
    bool  pedalMode  = false; // true → adds InputBuffer + OutputStage coloration
};
```

### Key Methods

```cpp
void prepare(const patina::ProcessSpec& spec) noexcept;
void reset() noexcept;
void gateOn()  noexcept;   // External trigger: start attack
void gateOff() noexcept;   // External trigger: start release
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params) noexcept;
float getEnvelopeLevel() const noexcept;  // Current envelope value (0.0–1.0)
```

**Usage:**
```cpp
patina::EnvelopeGeneratorEngine env;
patina::ProcessSpec spec{48000.0, 256, 2};
env.prepare(spec);

env.gateOn();   // trigger from MIDI note-on

patina::EnvelopeGeneratorEngine::Params p;
p.attack  = 0.2f;
p.decay   = 0.4f;
p.sustain = 0.6f;
p.release = 0.5f;
env.processBlock(inPtrs, outPtrs, 2, 256, p);
```

---

## 74. ModulationEngine

`#include "dsp/engine/ModulationEngine.h"`

Phaser / Tremolo / Chorus integrated modulation engine.

> Signal path:  
> Outboard: Input → [Phaser|Tremolo|Chorus] → Dry/Wet  
> Pedal: Input → **InputBuffer** → [Phaser|Tremolo|Chorus] → **OutputStage** → Dry/Wet

### `enum Type`
| Value | Description |
|---|---|
| `Phaser` | JFET allpass phaser (2–12 stage cascade) |
| `Tremolo` | Bias / Optical / VCA tremolo |
| `Chorus` | Short delay + LFO modulation + stereo width |

### `struct Params`
```cpp
struct Params {
    int   type          = Phaser;   // Effect type
    float rate          = 0.5f;     // LFO rate (Hz) 0.1–10.0
    float depth         = 0.5f;     // Depth 0.0–1.0
    float feedback      = 0.3f;     // Feedback (Phaser)
    float mix           = 0.5f;     // Dry/Wet 0.0–1.0
    double supplyVoltage = 9.0;
    // Phaser-specific
    float centerFreqHz  = 1000.0f;
    float freqSpreadHz  = 800.0f;
    int   numStages     = 4;        // 2–12
    float temperature   = 25.0f;
    // Tremolo-specific
    int   tremoloMode   = 0;        // 0=Bias, 1=Optical, 2=VCA
    bool  stereoPhaseInvert = false;
    // Chorus-specific
    float chorusDelayMs = 7.0f;     // Base delay (ms)
    float stereoWidth   = 0.5f;     // Stereo width 0.0–1.0

    bool  pedalMode      = false;   // true=pedal quality, false=outboard quality
};
```

### Methods

```cpp
void prepare(const patina::ProcessSpec& spec);
void reset();
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params);
```

**Typical usage:**
```cpp
patina::ModulationEngine mod;
mod.prepare({48000.0, 512, 2});

patina::ModulationEngine::Params p;
p.type = patina::ModulationEngine::Chorus;
p.rate = 1.0f;
p.depth = 0.5f;
p.stereoWidth = 0.7f;
p.mix = 0.5f;
mod.processBlock(input, output, 2, blockSize, p);
```

---

## 75. TapeMachineEngine

`#include "dsp/engine/TapeMachineEngine.h"`

Full-chain simulation of a studio tape machine.

> Signal path:  
> Outboard: Input → TapeSaturation → TransformerModel(output transformer) → ToneFilter → Dry/Wet  
> Pedal: Input → **InputBuffer** → TapeSaturation → TransformerModel(output transformer)  
> → ToneFilter → **OutputStage** → Dry/Wet

### `struct Params`
```cpp
struct Params {
    float inputGain      = 0.0f;    // Input gain
    float saturation     = 0.5f;    // Saturation 0.0–1.0
    float biasAmount     = 0.5f;    // Bias amount 0.0–1.0
    float tapeSpeed      = 1.0f;    // 0.5=7.5ips, 1.0=15ips, 2.0=30ips
    float wowFlutter     = 0.0f;    // Wow & flutter 0.0–1.0
    bool  enableHeadBump = true;
    bool  enableHfRolloff = true;
    float headWear       = 0.0f;    // Head wear 0.0–1.0
    float tapeAge        = 0.0f;    // Tape degradation 0.0–1.0
    bool  enableTransformer = true;
    float transformerDrive  = 0.0f; // (dB)
    float transformerSat    = 0.3f; // 0.0–1.0
    float tone           = 0.5f;    // Tone 0.0–1.0
    float mix            = 1.0f;    // Dry/Wet 0.0–1.0
    double supplyVoltage = 9.0;

    bool  pedalMode      = false;   // true=pedal quality, false=outboard quality
};
```

### Methods

```cpp
void prepare(const patina::ProcessSpec& spec);
void reset();
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params);
```

**Typical usage:**
```cpp
patina::TapeMachineEngine tape;
tape.prepare({48000.0, 512, 2});

patina::TapeMachineEngine::Params p;
p.saturation = 0.6f;
p.tapeSpeed = 1.0f;    // 15ips
p.wowFlutter = 0.2f;
p.enableTransformer = true;
p.transformerSat = 0.4f;
tape.processBlock(input, output, 2, blockSize, p);
```

---

## 76. ChannelStripEngine

`#include "dsp/engine/ChannelStripEngine.h"`

Analog console-style integrated channel strip. Defaults to transparent insert processing
with a simple 3-function configuration: preamp, EQ, and gate.

> Signal path:  
> Outboard: Input → InputTrim → NoiseGate(opt) → TubePreamp(12AX7)  
> → StateVariableFilter(EQ) → OutputTrim  
> Pedal: Input → **InputBuffer** → InputTrim → NoiseGate(opt) → TubePreamp(12AX7)  
> → StateVariableFilter(EQ) → OutputTrim → **OutputStage**  
> (EnvelopeFollower for metering)

### `struct Params`
```cpp
struct Params {
    // Preamp
    float preampDrive    = 0.15f;   // Drive 0.0–1.0
    float preampBias     = 0.5f;    // Bias point 0.0–1.0
    float preampOutput   = 0.5f;    // Output level 0.0–1.0
    float tubeAge        = 0.0f;    // Tube aging 0.0–1.0
    // EQ (StateVariableFilter)
    bool  enableEq       = false;   // Enable EQ (default: bypass)
    float eqCutoffHz     = 1000.0f; // Cutoff (Hz)
    float eqResonance    = 0.5f;    // Resonance 0.0–1.0
    int   eqType         = 0;       // 0=LP, 1=HP, 2=BP, 3=Notch
    float eqTemperature  = 25.0f;
    // Noise gate
    bool  enableGate      = false;
    float gateThresholdDb = -50.0f;
    float gateHysteresisDb = 6.0f;
    // Input/Output trim
    float inputTrimDb    = 0.0f;    // Input trim (dB)
    float outputTrimDb   = 0.0f;    // Output trim (dB)
    // Pedal mode
    bool  pedalMode      = false;   // true=pedal quality, false=outboard quality
    double supplyVoltage = 9.0;
};
```

### Methods

```cpp
void prepare(const patina::ProcessSpec& spec);
void reset();
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params);

// Metering
float getOutputLevel(int channel = 0) const;
bool  isGateOpen(int channel = 0) const;
```

**Typical usage:**
```cpp
patina::ChannelStripEngine strip;
strip.prepare({48000.0, 512, 2});

patina::ChannelStripEngine::Params p;
p.preampDrive = 0.5f;
p.enableEq = true;
p.eqCutoffHz = 3000.0f;
p.eqType = 0;  // LP
strip.processBlock(input, output, 2, blockSize, p);
float level = strip.getOutputLevel(0);  // Metering value
```

---

## 77. EqEngine

`#include "dsp/engine/EqEngine.h"`

3-band parametric EQ engine. OTA-SVF-based analog console-style equalizer.
Blends SVF outputs (LP/HP/BP) per band to construct shelf/bell characteristics.

> Signal path:  
> Outboard: Input → [LowShelf + MidBell + HighShelf (parallel summing)] → OutputGain  
> Pedal: Input → **InputBuffer** → [LowShelf + MidBell + HighShelf (parallel summing)] → **OutputStage**
>
> EQ method (parallel summing):
> Supplies the same input to all SVFs, adding only the gain difference. Prevents crossover interference.
> - Low Shelf:  `x += (gain - 1) × LP(x)`
> - Mid Bell:   `x += (gain - 1) × BP(x)`
> - High Shelf: `x += (gain - 1) × HP(x)`

### `struct Params`
```cpp
struct Params {
    // Low Shelf
    bool  enableLow      = true;
    float lowFreqHz      = 200.0f;    // 20–2000 Hz
    float lowGainDb      = 0.0f;      // -12 ~ +12 dB
    float lowResonance   = 0.3f;      // Shelf slope 0.0–1.0
    // Mid Bell (Parametric)
    bool  enableMid      = true;
    float midFreqHz      = 1000.0f;   // 100–10000 Hz
    float midGainDb      = 0.0f;      // -12 ~ +12 dB
    float midQ           = 0.5f;      // Q 0.1–1.0
    // High Shelf
    bool  enableHigh     = true;
    float highFreqHz     = 4000.0f;   // 1000–20000 Hz
    float highGainDb     = 0.0f;      // -12 ~ +12 dB
    float highResonance  = 0.3f;      // Shelf slope 0.0–1.0
    // Overall
    float temperature    = 25.0f;
    float outputGainDb   = 0.0f;      // -12 ~ +12 dB
    double supplyVoltage = 9.0;

    bool  pedalMode      = false;   // true=pedal quality, false=outboard quality
};
```

### Methods

```cpp
void prepare(const patina::ProcessSpec& spec);
void reset();
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params);
```

| Band | Implementation | Notes |
|--------|---------|------|
| Low Shelf | `x += (gain - 1) × LP(x)` | Parallel summing: adds gain difference of SVF LP output |
| Mid Bell | `x += (gain - 1) × BP(x)` | Parallel summing: adds gain difference of SVF BP output |
| High Shelf | `x += (gain - 1) × HP(x)` | Parallel summing: adds gain difference of SVF HP output |

**Typical usage:**
```cpp
patina::EqEngine eq;
eq.prepare({48000.0, 256, 2});

patina::EqEngine::Params p;
p.lowFreqHz = 150.0f;  p.lowGainDb = 3.0f;   // Low +3dB
p.midFreqHz = 800.0f;  p.midGainDb = -2.0f;  // Mid -2dB
p.highFreqHz = 6000.0f; p.highGainDb = 1.5f;  // High +1.5dB
eq.processBlock(input, output, 2, blockSize, p);
```

---

## Typical Full Chain Example

### High-Level API (BbdDelayEngine)

```cpp
#include "patina.h"

// Engine initialization
patina::BbdDelayEngine engine;
patina::ProcessSpec spec{48000.0, 256, 2};
engine.prepare(spec);

// Parts-swap MOD (optional)
ModdingConfig mod;
mod.opAmp     = ModdingConfig::JRC4558D;
mod.compander = ModdingConfig::NE570;
mod.capGrade  = ModdingConfig::Film;
engine.applyModdingConfig(mod);

// Block processing
patina::BbdDelayEngine::Params params;
params.delayMs     = 300.0f;
params.feedback    = 0.5f;
params.tone        = 0.6f;
params.mix         = 0.4f;
params.compAmount  = 0.7f;
params.chorusDepth = 0.2f;
params.lfoRateHz   = 0.8f;
engine.processBlock(input, output, 2, blockSize, params);
```

### Low-Level API (Individual Modules)

```cpp
// ============================================================
// BBD Analog Delay Chain (pseudocode)
// ============================================================

// 1. Input buffering
patina::ProcessSpec spec{sr, blockSize, 2};
InputBuffer inputBuf;
InputFilter         inputLpf;
inputBuf.prepare(spec);  inputLpf.prepare(spec);
float buffered = inputBuf.process(ch, inSample);
float filtered = inputLpf.process(ch, buffered);

// 2. Compander pre-stage compression
CompanderModule comp;
comp.prepare(spec);
float compressed = comp.processCompress(ch, filtered, compAmount);

// 3. Write to ring buffer & delay readout
ringBuf.setSample(ch, writePos, compressed);
float delayed = DelayLineView::readFromDelay(
    ringBuf, writePos, ch, delaySamples,
    PartsConstants::bbdStagesDefault, /*emulateBBD=*/true);

// 4. BBD tone coloring
std::vector<float> v(1, delayed);
bbd.process(v, 0.0, PartsConstants::bbdStagesDefault, supplyV, false, 0.0);
delayed = v[0];

// 5. Compander post-stage expansion
float expanded = comp.processExpand(ch, delayed, compAmount);

// 6. Output
OutputStage outMod;
outMod.prepare(spec);
float out = outMod.process(ch, expanded, supplyV);

// 7. Dry/Wet mix
float gDry, gWet;
Mixer::equalPowerGainsFast(mix, gDry, gWet);
float mixed = gDry * inSample + gWet * out;
```

---

## 78. LimiterEngine

`#include "dsp/engine/LimiterEngine.h"`

FET / VCA / Opto 3-mode switchable limiter integrated engine.
Unlike compressors, limits above threshold with a "ceiling" (fixed ultra-high ratio equivalent to ∞:1).

> Signal path:  
> Outboard: Input → [Limiter] → OutputCeiling → Dry/Wet  
> Pedal: Input → **InputBuffer** → [Limiter] → OutputCeiling → **OutputStage** → Dry/Wet

| Type | Model | Character |
|--------|--------|------|
| `Fet` (0) | Classic FET limiter | JFET VCA, ultra-fast attack, transformer coloration |
| `Vca` (1) | Console VCA ∞:1 | THAT2180, fixed hard knee, transparent |
| `Opto` (2) | Classic opto limiter | T4B photocell, program-dependent |

### `struct Params`
```cpp
struct Params {
    int   type            = Vca;        // 0=FET, 1=VCA, 2=Opto
    float ceiling         = 0.8f;       // Output ceiling 0.0–1.0 → -20dB ~ 0dBFS
    float attack          = 0.1f;       // Attack 0.0–1.0 (FET/VCA)
    float release         = 0.4f;       // Release 0.0–1.0
    float outputGain      = 0.5f;       // Makeup gain 0.0–1.0
    float mix             = 1.0f;       // Dry/Wet 0.0–1.0
    bool  pedalMode       = false;      // true=pedal quality, false=outboard quality
    double supplyVoltage  = 9.0;        // Supply voltage for pedal mode
};
```

### Methods

```cpp
void prepare(const patina::ProcessSpec& spec);
void reset();
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params);

// Gain reduction (dB)
float getGainReductionDb(int channel = 0) const;
float getFetGainReductionDb(int channel = 0) const;
float getVcaGainReductionDb(int channel = 0) const;
float getOptoGainReductionDb(int channel = 0) const;
```

**Typical usage:**
```cpp
patina::LimiterEngine lim;
lim.prepare({48000.0, 256, 2});

patina::LimiterEngine::Params p;
p.type    = patina::LimiterEngine::Vca;
p.ceiling = 0.8f;    // -4 dBFS
p.attack  = 0.1f;
p.release = 0.4f;
lim.processBlock(input, output, 2, blockSize, p);
float gr = lim.getVcaGainReductionDb(0);
```

---

## 79. FilterEngine

`#include "dsp/engine/FilterEngine.h"`

Dual filter + triple drive integrated engine.
Combines 2 filters and 3 drives in serial / parallel routing
to build versatile filter effects.

> Signal path:  
> Serial:   Input → Drive1 → Filter1 → Drive2 → Filter2 → Drive3 → Dry/Wet  
> Parallel: Input → Drive1 → [Filter1 | Filter2] mix → Drive3 → Dry/Wet  
> Pedal: Adds InputBuffer / OutputStage to each mode

### Filter Type

| Type | enum | Model | Character |
|--------|------|--------|------|
| `LowPass` (0) | SVF | OTA-SVF 2-pole | LP / variable slope |
| `HighPass` (1) | SVF | OTA-SVF 2-pole | HP / variable slope |
| `BandPass` (2) | SVF | OTA-SVF 2-pole | BP / variable slope |
| `Ladder` (3) | LadderFilter | BJT ladder 4-pole | LP fixed -24dB/oct |

### Slope

| Slope | enum | Implementation |
|----------|------|------|
| `-6dB/oct` (0) | `Slope_6dB` | ToneFilter 1-pole (HPF = x − LP) |
| `-12dB/oct` (1) | `Slope_12dB` | 1× SVF 2-pole |
| `-18dB/oct` (2) | `Slope_18dB` | SVF + ToneFilter cascade (3-pole) |
| `-24dB/oct` (3) | `Slope_24dB` | 2× SVF cascade (4-pole) |

> When Ladder type is selected, slope is always -24dB/oct regardless of setting.

### Drive Type

| Type | enum | Model |
|--------|------|--------|
| `Tube` (0) | TubePreamp | 12AX7 tube preamp |
| `Diode` (1) | DiodeClipper | Si/Ge diode clipper |
| `Wave` (2) | WaveFolder | Wavefolder |
| `Tape` (3) | TapeSaturation | Magnetic tape saturation |

### Gain Compensation (Normalization)

Built-in normalizer that automatically compensates for volume loss from filter band-cutting.
Applies up to ~8dB of gain compensation depending on cutoff frequency, slope, and filter type.
`Params::normalize` (default `true`) can toggle ON/OFF.

- **LPF / Ladder**: Compensation increases as cutoff lowers
- **HPF**: Compensation increases as cutoff raises
- **BPF**: Uniform compensation for band-limiting loss (adjusted by resonance)
- Steeper slopes result in larger compensation coefficients
- Setting `normalize = false` bypasses gain compensation, yielding the filter's native volume characteristic

### `struct Params`
```cpp
struct Params {
    int   routing           = 0;        // 0=Serial, 1=Parallel
    float filter1CutoffHz   = 1000.0f;  // 20–20000 Hz
    float filter1Resonance  = 0.5f;     // 0.0–1.0
    int   filter1Type       = 0;        // 0=LP, 1=HP, 2=BP, 3=Ladder
    int   filter1Slope      = 1;        // 0=-6dB, 1=-12dB, 2=-18dB, 3=-24dB
    float filter2CutoffHz   = 2000.0f;  // 20–20000 Hz
    float filter2Resonance  = 0.5f;     // 0.0–1.0
    int   filter2Type       = 0;        // 0=LP, 1=HP, 2=BP, 3=Ladder
    int   filter2Slope      = 1;        // 0=-6dB, 1=-12dB, 2=-18dB, 3=-24dB
    float drive1Amount      = 0.0f;     // 0.0–1.0
    int   drive1Type        = 0;        // 0=Tube, 1=Diode, 2=Wave, 3=Tape
    float drive2Amount      = 0.0f;     // 0.0–1.0 (Serial only)
    int   drive2Type        = 0;
    float drive3Amount      = 0.0f;     // 0.0–1.0
    int   drive3Type        = 0;
    float outputLevel       = 0.7f;     // 0.0–1.0
    float mix               = 1.0f;     // Dry/Wet 0.0–1.0
    float temperature       = 25.0f;    // °C
    double supplyVoltage    = 9.0;      // Supply voltage for pedal mode
    bool  pedalMode         = false;    // true=pedal quality
    bool  normalize         = true;     // true=gain compensation enabled
};
```

### Methods

```cpp
void prepare(const patina::ProcessSpec& spec);
void reset();
void processBlock(const float* const* input, float* const* output,
                  int numChannels, int numSamples, const Params& params);

const StateVariableFilter& getFilter1() const;   // For diagnostics
const StateVariableFilter& getFilter2() const;
```

**Typical usage:**
```cpp
patina::FilterEngine filt;
filt.prepare({48000.0, 256, 2});

patina::FilterEngine::Params p;
p.routing         = patina::FilterEngine::Serial;
p.filter1CutoffHz = 800.0f;
p.filter1Type     = patina::FilterEngine::LowPass;
p.filter1Slope    = patina::FilterEngine::Slope_18dB;
p.filter2CutoffHz = 300.0f;
p.filter2Type     = patina::FilterEngine::HighPass;
p.filter2Slope    = patina::FilterEngine::Slope_12dB;
p.drive1Amount    = 0.4f;
p.drive1Type      = patina::FilterEngine::Tube;
p.outputLevel     = 0.7f;
filt.processBlock(input, output, 2, blockSize, p);
```

---

# Config

> Cross-layer configuration and preset mechanisms.

---

## 80. ModdingConfig

`#include "dsp/config/ModdingConfig.h"`

Data model for parts-swap mod simulation.

### Enums
- `OpAmpType`: `TL072CP`, `JRC4558D`, `OPA2134`, `LM4562`, `NE5532`, `LM741`
- `CompanderType`: `NE570`, `SA571N`
- `CapGrade`: `Standard`, `Film`, `AudioGrade`

### Fields
```cpp
int opAmp     = TL072CP;   // Op-amp selection
int compander = NE570;     // Compander IC selection
int capGrade  = Standard;  // Capacitor grade selection
```

`OpAmpSpec` is a type alias for `OpAmpPrimitive::Spec`. Contains specification tables for each `OpAmpSpec`, `CompanderSpec`, and `CapGradeSpec`, providing parameters such as slew rate, open-loop gain, and noise scale.

---

## 81. AnalogPresets

`#include "dsp/config/Presets.h"`

`patina::presets` namespace. Factory functions that generate module Config/Params from ModdingConfig spec tables in a single call.

### Compander IC Presets

```cpp
CompanderModule::Config ne570();   // NE570 default (VCA output ratio 0.6)
CompanderModule::Config sa571n();  // SA571N (VCA output ratio 0.65, fast timing)
```

### Op-Amp Presets

```cpp
ModdingConfig::OpAmpSpec tl072cp();   // TL072CP (standard)
ModdingConfig::OpAmpSpec jrc4558d();  // JRC4558D (low slew rate, warmth)
ModdingConfig::OpAmpSpec opa2134();   // OPA2134 (precision FET input)
ModdingConfig::OpAmpSpec lm4562();    // LM4562 (ultra-low noise)
ModdingConfig::OpAmpSpec ne5532();    // NE5532 (bipolar, classic)
ModdingConfig::OpAmpSpec lm741();     // LM741 (1968 vintage)
ModdingConfig::OpAmpSpec opAmpFromType(int opAmpType);  // Convert from enum
```

> `ModdingConfig::OpAmpSpec` is a type alias for `OpAmpPrimitive::Spec`. For backward compatibility, `BbdFeedback::OpAmpOverrides` is also available as the same type alias.

### BBD Chip Presets

```cpp
BbdStageEmulator::Params mn3005(double supplyVoltage = 15.0);  // MN3005 (4096 stages)
BbdStageEmulator::Params mn3207(double supplyVoltage = 9.0);   // MN3207 (1024 stages)
BbdStageEmulator::Params mn3005Dual(double supplyVoltage = 15.0); // MN3005×2 (8192 stages)
```

### Capacitor Grade

```cpp
double capGradeBandwidthScale(int capGrade);  // CapGrade → bandwidth scale value
```
Calculates values for `BbdStageEmulator::setBandwidthScale()` from the `ModdingConfig::CapGrade` enum.  
`Standard=1.0, Film≈1.41, AudioGrade≈2.0`。

**Typical usage:**
```cpp
using namespace patina::presets;

CompanderModule comp;
comp.setConfig(ne570());            // Apply NE570 preset

BbdFeedback fb;
fb.setOpAmpOverrides(jrc4558d());   // JRC4558D warm character (OpAmpPrimitive::Spec)

BbdStageEmulator emu;
auto p = mn3005Dual();              // Classic BBD delay unit configuration
emu.setBandwidthScale(capGradeBandwidthScale(ModdingConfig::Film));
```

---

# Language Bindings

---

## 82. C API (patina_c.h)

`#include "patina_c.h"`

Opaque handle-based interface making all 7 engines available from C.
`bindings/c/patina_c.h` (header) + `bindings/c/patina_c.cpp` (implementation).

### Common Types

```c
typedef struct {
    double sample_rate;
    int    max_block_size;
    int    num_channels;
} PatinaProcessSpec;
```

### Lifecycle (common pattern for all engines)

```c
// 1. Create
PatinaDelayEngine engine = patina_delay_create();

// 2. Initialize
PatinaProcessSpec spec = { 48000.0, 256, 2 };
patina_delay_prepare(engine, &spec);

// 3. Set parameters
PatinaDelayParams params = patina_delay_default_params();
params.delay_ms = 300.0f;

// 4. Process
patina_delay_process(engine, input, output, 2, 256, &params);

// 5. Reset (clear state)
patina_delay_reset(engine);

// 6. Destroy
patina_delay_destroy(engine);
```

### Per-Engine API List

| Engine | Handle Type | Params Type | Additional Functions |
|---|---|---|---|
| BBD Delay | `PatinaDelayEngine` | `PatinaDelayParams` | — |
| Drive | `PatinaDriveEngine` | `PatinaDriveParams` | — |
| Reverb | `PatinaReverbEngine` | `PatinaReverbParams` | — |
| Compressor | `PatinaCompressorEngine` | `PatinaCompressorParams` | `_get_gain_reduction_db()`, `_is_gate_open()` |
| Modulation | `PatinaModulationEngine` | `PatinaModulationParams` | — |
| Tape | `PatinaTapeEngine` | `PatinaTapeParams` | — |
| Channel Strip | `PatinaChannelStrip` | `PatinaChannelStripParams` | `_get_output_level()`, `_is_gate_open()` |

### Common Parameter: pedal_mode

All 7 Params structs have an `int pedal_mode` field.

| Value | Behavior |
|---|---|
| `0` (default) | Outboard quality — bypasses InputBuffer / OutputStage |
| `1` | Pedal quality — enables TL072 input buffer + OutputStage (soft clipping by supply voltage) |

### Safety Guards

- `_process()` returns immediately without action when `input == NULL`, `output == NULL`, `num_channels <= 0`, or `num_samples <= 0`
- Handle is invalid after `_destroy()`. The caller should set it to `NULL`

### Enumeration Constants

```c
// Reverb
PATINA_REVERB_SPRING (0), PATINA_REVERB_PLATE (1)

// Compressor
PATINA_COMP_PHOTO (0), PATINA_COMP_FET (1), PATINA_COMP_VARIABLE_MU (2)

// Modulation
PATINA_MOD_PHASER (0), PATINA_MOD_TREMOLO (1), PATINA_MOD_CHORUS (2)
```

### Build

```bash
cmake -DPATINA_BUILD_C_BINDINGS=ON ..
cmake --build .
# → libpatina_c.a (static) + libpatina_c.dylib/so (shared)
```

### DLL Export (Windows)

```c
// Define PATINA_SHARED when using as a shared library
#define PATINA_SHARED
#include "patina_c.h"
```

---

## 83. Rust Crate (patina-dsp)

`bindings/rust/` — Safe Rust wrapper for `patina_c.h`.

### Crate Structure

```
bindings/rust/
├── Cargo.toml      # patina-dsp crate definition
├── build.rs        # Auto-compiles patina_c.cpp via cc crate
└── src/
    ├── ffi.rs      # Raw extern "C" declarations (1:1 with patina_c.h)
    └── lib.rs      # Safe wrapper types
```

### Engine Types

| Rust Type | Params Type | Corresponding C++ Class |
|---|---|---|
| `BbdDelayEngine` | `DelayParams` | `patina::BbdDelayEngine` |
| `DriveEngine` | `DriveParams` | `patina::DriveEngine` |
| `ReverbEngine` | `ReverbParams` | `patina::ReverbEngine` |
| `CompressorEngine` | `CompressorParams` | `patina::CompressorEngine` |
| `ModulationEngine` | `ModulationParams` | `patina::ModulationEngine` |
| `TapeMachineEngine` | `TapeParams` | `patina::TapeMachineEngine` |
| `ChannelStripEngine` | `ChannelStripParams` | `patina::ChannelStripEngine` |

### Usage

```rust
use patina::{ProcessSpec, ReverbEngine, ReverbParams, PATINA_REVERB_PLATE};

let mut engine = ReverbEngine::new().unwrap();
engine.prepare(&ProcessSpec {
    sample_rate: 48000.0,
    max_block_size: 512,
    num_channels: 2,
});

let mut params = ReverbParams::default();
params.r#type = PATINA_REVERB_PLATE;
params.decay = 0.7;
params.mix = 0.3;

let input = vec![vec![0.0f32; 512]; 2];
let mut output = vec![vec![0.0f32; 512]; 2];
engine.process(&input, &mut output, &params);
// engine is automatically released via Drop
```

### Safety

- `new()` → `Option<Self>`: Returns `None` on memory allocation failure
- Automatic deallocation via `Drop` (RAII)
- Implements `Send` (transferable between threads assuming external synchronization)
- Implements `Default` (all Params types)

### Build

```bash
cd bindings/rust
cargo build --release
cargo test  # Parameter default value tests
```

`build.rs` uses the `cc` crate to automatically compile `patina_c.cpp`,
so a prior CMake build is not required.
