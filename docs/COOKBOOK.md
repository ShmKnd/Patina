
# Patina Cookbook — L2 / L3 Practical Recipes

This is a practical guide for designing your own circuits using L2 Parts (analog primitives) and L3 Circuits (circuit modules).

> **Prerequisites**: Include `include/patina.h` to access all layers.
> Build: `c++ -std=c++17 -O2 -I. your_file.cpp -o your_app`

---

## Table of Contents

1. [Basic Pattern — ProcessSpec and Initialization](#1-basic-pattern)
2. [Signal Level Conventions](#2-signal-level-conventions)
3. [L2 State Management Guide](#3-l2-state-management-guide)
4. [Recipe 1: Hand-built Transistor Ladder Filter (L2)](#recipe-1)
5. [Recipe 2: Building OTA-SVF from Primitives (L2)](#recipe-2)
6. [Recipe 3: Overdrive Circuit with L3 Modules](#recipe-3)
7. [Recipe 4: LFO → Filter Modulation (L3)](#recipe-4)
8. [Recipe 5: Phaser + Tremolo Combined Effect (L3)](#recipe-5)
9. [Recipe 6: Hand-built BBD Delay (L3)](#recipe-6)
10. [Recipe 7: Compressor Chain — Photo → FET Cascade (L3)](#recipe-7)
11. [Recipe 8: Tape Saturation + Transformer for “Mastering Glue” (L3)](#recipe-8)
12. [Recipe 9: Opto Comp VCA from L2 Parts (L2)](#recipe-9)
13. [Recipe 10: M/S Iron — Transformer Method (L3)](#recipe-10)
14. [Recipe 11: M/S Precision — Op-Amp Method (L3)](#recipe-11)
15. [Recipe 12: Passive LC Filter — Inductor Harmonics (L3)](#recipe-12)
16. [Recipe 13: Push-Pull Power Stage — Tube Amp Output (L3)](#recipe-13)
17. [Parameter Routing Quick Reference](#parameter-routing)
18. [Troubleshooting](#troubleshooting)

---

<a id="1-basic-pattern"></a>
## 1. Basic Pattern — ProcessSpec and Initialization

```cpp
#include "include/patina.h"

// Common parameters
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 256;
constexpr int    kChannels   = 2;

patina::ProcessSpec spec{kSampleRate, kBlockSize, kChannels};
```

### L3 Module Initialization Template

```cpp
// 1. Declaration
StateVariableFilter svf;
ToneFilter         tone;

// 2. prepare() — Calculates internal coefficients depending on sample rate
svf.prepare(kChannels, kSampleRate);   // SVF は (numCh, sr)
tone.prepare(kSampleRate, kChannels, kBlockSize);

// 3. Parameter setting
svf.setCutoffHz(1000.0f);
svf.setResonance(0.7f);
tone.setDefaultCutoff(2000.0f);

// 4. reset() — Clears state (call at song start or when bypass is disengaged)
svf.reset();
tone.reset();
```

### L2 Primitive Initialization Template

```cpp
// Stateless type (DiodePrimitive, JFET_Primitive)
DiodePrimitive si(DiodePrimitive::Si1N4148());
// → prepare() not required. You can call si.clip(x, temperature) directly

// Stateful type (TubeTriode, RC_Element, TransformerPrimitive, etc.)
TubeTriode tube(TubeTriode::T12AX7());
tube.prepare(kSampleRate);   // Required: Calculates Miller/plateCap alpha coefficients
tube.reset();                // State initialization

RC_Element rc(10000.0, 3.3e-9);  // R=10kΩ, C=3.3nF
rc.prepare(kSampleRate);
rc.reset();

// Hybrid type (OTA_Primitive, BJT_Primitive)
OTA_Primitive ota(OTA_Primitive::LM13700(), /*seed=*/42);
// → prepare() not required, but the state for integrate() is managed by the caller
double state = 0.0;  // ← This is the filter's "state"
```

---

<a id="2-signal-level-conventions"></a>
## 2. Signal Level Conventions

| Domain | Level | Notes |
|--------|-------|-------|
| Digital I/O | ±1.0 (0 dBFS) | WAV export / DAW buffer |
| L3 Internal | ±1.0 normalized | processBlock input/output |
| L2 プリミティブ | ±1.0 正規化 | clip(), saturate() 等 |
| Voltage Domain Simulation | ±4V (9V supply) / ±8V (18V supply) | Converted by InputBuffer, OutputStage |
| LFO Output | -1.0 to +1.0 | getTri(), getSinLike() |
| Envelope Output | 0.0 to 1.0 | EnvelopeFollower, PhotocellPrimitive |
| Dry/Wet | 0.0 (dry) to 1.0 (wet) | DryWetMixer |

### Note: per-sample vs per-block

```cpp
// per-sample (most modules)
for (int i = 0; i < numSamples; ++i) {
    float y = module.process(ch, input[i], params);
}

// per-block (some modules — if processBlock exists)
module.processBlock(io, numChannels, numSamples, params);

// LFO requires per-sample stepping
for (int i = 0; i < numSamples; ++i) {
    lfo.stepAll();                     // Advance all channels by 1 sample
    float tri = lfo.getTri(ch);        // Get current value
}
```

---

<a id="3-l2-state-management-guide"></a>
## 3. L2 State Management Guide

### Classification Table

| Primitive | State | prepare? | reset? | Creation Method |
|---|---|---|---|---|
| DiodePrimitive | None | Not required | Not required | `Spec` or factory |
| JFET_Primitive | None (mismatch is const) | Not required | Not required | factory + seed |
| OTA_Primitive | **External** | Not required | Not required | factory + seed |
| BJT_Primitive | **External** | Not required | Not required | factory + seed |
| TubeTriode | **Internal** (miller, plate, grid) | **Required** | **Required** | factory |
| TransformerPrimitive | **Internal** (10+ variables) | **Required** | **Required** | factory |
| TapePrimitive | **Internal** (hystState, pink) | Not required | **Required** | factory |
| RC_Element | **Internal** (LPF/HPF state) | **Required** | **Required** | R, C values |
| PhotocellPrimitive | **Internal** (EL, CdS, memory) | **Required** | **Required** | factory |

### How to Use External State (OTA / BJT)

`integrate()` requires the caller to manage the state variable.
This allows you to build multi-stage filters with the same OTA/BJT instance.

```cpp
OTA_Primitive ota(OTA_Primitive::LM13700(), 42);

// 2-stage filter → need two states
double s1 = 0.0;  // 1st stage
double s2 = 0.0;  // 2nd stage

// g coefficient: calculated from cutoff frequency
double fc = 1000.0;  // Hz
double g = std::tan(M_PI * fc / kSampleRate);
// ↑ Pre-warping for bilinear transform

for (int i = 0; i < numSamples; ++i) {
    s1 = ota.integrate(input[i], s1, g);   // 1st stage LP
    s2 = ota.integrate(s1,       s2, g);   // 2nd stage LP (cascade)
    output[i] = (float)s2;
}
```

### How to Derive gCoeff

```cpp
// Basic: bilinear transform
double g = std::tan(M_PI * cutoffHz / sampleRate);

// With temperature compensation (OTA)
double gTemp = g * ota.gmScale(temperature);
//                  ↑ Adjust gm for deviation from 25°C (−0.3%/°C)

// For BJT: Vbe temperature drift
double gBjt = g * bjt.tempScale(temperature);
//                  ↑ Frequency shift of −0.2%/°C
```

### Multi-channel State Management

```cpp
constexpr int kCh = 2;

// State arrays for each channel
double s1[kCh] = {};  // zero-init
double s2[kCh] = {};

for (int i = 0; i < numSamples; ++i) {
    for (int ch = 0; ch < kCh; ++ch) {
        s1[ch] = ota.integrate(input[ch][i], s1[ch], g);
        s2[ch] = ota.integrate(s1[ch],       s2[ch], g);
        output[ch][i] = (float)s2[ch];
    }
}
```

### When to Reset

```cpp
// Song start: zero all states
tube.reset();
rc.reset();
s1 = s2 = 0.0;  // external state

// Bypass → enabled: prevent pop noise
tape.reset();
transformer.reset();

// Sample rate change: prepare() → reset()
rc.prepare(newSampleRate);
rc.reset();
```

---

<a id="recipe-1"></a>
## Recipe 1: Hand-built Transistor Ladder Filter (L2)

Build a Moog-style ladder filter using four BJT differential pairs + RC. Equivalent to L3 `LadderFilter`, but controlled at the primitive level.

```cpp
#include "include/patina.h"
#include <cmath>

constexpr int kStages = 4;
constexpr double kSampleRate = 48000.0;

// Prepare components
BJT_Primitive bjt[kStages] = {
    {BJT_Primitive::Matched(), 101},
    {BJT_Primitive::Matched(), 102},
    {BJT_Primitive::Matched(), 103},
    {BJT_Primitive::Matched(), 104}
};

RC_Element rc[kStages] = {
    {10000.0, 3.3e-9},   // Each stage fc ≈ 4.8kHz (open)
    {10000.0, 3.3e-9},
    {10000.0, 3.3e-9},
    {10000.0, 3.3e-9}
};

void prepare() {
    for (int i = 0; i < kStages; ++i) {
        rc[i].prepare(kSampleRate);
        rc[i].reset();
    }
}

// State
double stageState[kStages] = {};
double capState[kStages - 1] = {};   // Inter-stage coupling

float processLadder(float input, float cutoffHz, float resonance,
                    float temperature = 25.0f)
{
    // g coefficient: pre-warped from cutoff
    double g = std::tan(M_PI * cutoffHz / kSampleRate);

    // Resonance feedback (4 stages → max k ≈ 4.0)
    double fb = resonance * 4.0;
    double fbSample = stageState[kStages - 1];

    // Subtract feedback from input
    double x = input - fb * BJT_Primitive::saturate(fbSample);

    // 4-stage serial processing
    for (int s = 0; s < kStages; ++s) {
        // BJT integration (includes gm mismatch)
        double gAdj = g * bjt[s].tempScale(temperature);
        stageState[s] = bjt[s].integrate(x, stageState[s], gAdj);

        // Inter-stage coupling (DC block + memory effect)
        if (s < kStages - 1) {
            stageState[s] = bjt[s].interStageCoupling(
                stageState[s], capState[s]);
        }

        // Add 1st-order LPF color with RC_Element
        x = rc[s].processLPF((float)stageState[s]);
    }

    return (float)x;
}
```


**Notes:**
- `g = tan(π × fc / sr)` is pre-warping for bilinear transform
- `BJT_Primitive::saturate()` is tanh saturation for feedback (prevents oscillation)
- `interStageCoupling()` is capacitor-coupled memory (99.8% pass + 0.2% previous sample)
- Each BJT has a different seed, so gm mismatch naturally occurs per stage

---

<a id="recipe-2"></a>
## Recipe 2: Building OTA-SVF from Primitives (L2)

Build a State Variable Filter using two OTA_Primitives.
This recipe helps understand the internal operation of the L3 `StateVariableFilter`.

```cpp
#include "include/patina.h"
#include <cmath>

constexpr double kSr = 48000.0;

// Two OTAs (different seeds cause mismatch)
OTA_Primitive ota1(OTA_Primitive::LM13700(), 42);
OTA_Primitive ota2(OTA_Primitive::LM13700(), 43);

// External state (per channel)
struct SvfState {
    double ic1 = 0.0;   // Integrator 1 (BP output)
    double ic2 = 0.0;   // Integrator 2 (LP output)
};

struct SvfOutput {
    float lp, hp, bp, notch;
};

SvfOutput processSvf(SvfState& s, float input,
                     float cutoffHz, float resonance,
                     float temperature = 25.0f)
{
    double g = std::tan(M_PI * cutoffHz / kSr);
    double gT = g * ota1.gmScale(temperature);

    // SVF topology (Hal Chamberlin / Andy Simper method)
    // Q = 1 / (2 × damping)
    double k = 2.0 - 2.0 * resonance;  // damping: 2.0(Q=0.5) → 0.0(self-osc)
    k = std::max(k, 0.05);             // Safety limit

    // HP = input - k × BP - LP
    double hp = input - k * s.ic1 - s.ic2;

    // BP = g × HP + ic1  (OTA integrator 1)
    s.ic1 = ota1.integrate(hp, s.ic1, gT);
    double bp = s.ic1;

    // LP = g × BP + ic2  (OTA integrator 2)
    s.ic2 = ota2.integrate(bp, s.ic2, gT);
    double lp = s.ic2;

    double notch = hp + lp;

    return {(float)lp, (float)hp, (float)bp, (float)notch};
}
```

**Notes:**
- OTA saturation (`saturate()` is called inside `integrate()`) produces natural analog feel
- gm mismatch between `ota1` and `ota2` (±1.5%) creates subtle left/right differences
- Setting `k` close to 0 causes self-oscillation (same as hardware)

---

<a id="recipe-3"></a>
## Recipe 3: Overdrive Circuit with L3 Modules

Reproduce the internal processing of L4 `DriveEngine` using L3 modules.

```cpp
#include "include/patina.h"

constexpr double kSr = 48000.0;
constexpr int kCh = 2;

// L3 modules
InputBuffer   inputBuf;
DiodeClipper  clipper;
ToneFilter    tone;
OutputStage   output;

void prepare() {
    patina::ProcessSpec spec{kSr, 256, kCh};
    inputBuf.prepare(spec);
    clipper.prepare(spec);
    tone.prepare(kSr, kCh, 256);
    output.prepare(spec);
}

// Parameters
struct DriveParams {
    float drive       = 0.6f;   // 0–1
    int   diodeType   = 0;      // 0=Si, 1=Schottky, 2=Ge
    float toneKnob    = 0.5f;   // 0–1
    float outputLevel = 0.7f;   // 0–1
    float mix         = 1.0f;   // 0(dry)–1(wet)
    float supplyV     = 9.0f;   // 9 or 18
    float temperature = 25.0f;
};

float processDrive(int ch, float x, const DriveParams& p)
{
    float dry = x;

    // 1. Input buffer (TL072 Hi-Z + cable capacitance)
    float wet = inputBuf.process(ch, x);

    // 2. Diode clipper
    wet = clipper.process(ch, wet, p.drive, /*mode=*/1);
    //                                       ↑ 1=Diode

    // 3. Tone filter (RC LPF)
    tone.updateToneFilterIfNeeded(p.toneKnob, /*agingCapScale=*/1.0, true);
    wet = tone.processSample(ch, wet);

    // 4. Output stage (3-pole LPF + soft saturation)
    wet = output.process(ch, wet, p.supplyV);

    // 5. Output level
    wet *= p.outputLevel;

    // 6. Dry/Wet mix (equal power)
    float gDry, gWet;
    DryWetMixer::equalPowerGainsFast(p.mix, gDry, gWet);
    return dry * gDry + wet * gWet;
}
```

**Notes:**
- `InputBuffer` delegates to `OpAmpPrimitive` to model slew rate limiting and offset drift
- `DiodeClipper` with mode=1 is RC HP → diode saturation → junction capacitance
- `ToneFilter` maps 500–8000Hz to a 0–1 parameter
- `OutputStage` headroom saturation depends on supply voltage

### Customization Example: Switching Diode Type

```cpp
// DiodeClipper uses DiodePrimitive internally
// Select Si/Schottky/Ge with diodeType parameter
DiodeClipper::Params clipParams;
clipParams.drive = 0.8f;
clipParams.mode = 1;                    // Diode mode
clipParams.diodeType = 2;              // 2 = Ge (GeOA91) → warm, asymmetric clip
clipParams.temperature = 35.0f;        // Higher temp → lower Vf → earlier clip

float y = clipper.process(ch, x, clipParams);
```

---

<a id="recipe-4"></a>
## Recipe 4: LFO → Filter Modulation (L3)

Auto-wah / filter sweep by modulating filter cutoff with AnalogLfo.

```cpp
#include "include/patina.h"
#include <cmath>

constexpr double kSr = 48000.0;
constexpr int kCh = 2;

AnalogLfo           lfo;
StateVariableFilter svf;

void prepare() {
    patina::ProcessSpec spec{kSr, 256, kCh};
    lfo.prepare(kCh, kSr);
    svf.prepare(kCh, kSr);

    lfo.setRateHz(2.0);         // 2Hz スイープ
    svf.setResonance(0.7f);     // レゾナンス高め
}

void processBlock(float** io, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        lfo.stepAll();   // Advance LFO per-sample

        for (int ch = 0; ch < kCh; ++ch)
        {
            // Get LFO value (−1 to +1)
            float tri = lfo.getTri(ch);
            //   ↑ ch=0 and ch=1 are quadrature (≈90° phase difference)

            //  LFO → cutoff frequency mapping
            //  Center 800Hz, ±600Hz sweep
            float fc = 800.0f + tri * 600.0f;  // 200–1400Hz

            svf.setCutoffHz(fc);
            io[ch][i] = svf.process(ch, io[ch][i], /*type=*/0);
            //                                       ↑ 0=LowPass
        }
    }
}
```

### LFO → Parameter Mapping Quick Reference


```
LFO output (−1 to +1)
    ↓
Linear mapping:
    param = center + lfo * depth
    Example: fc = 1000 + tri * 500  → 500–1500Hz

Logarithmic mapping (suitable for frequency):
    param = center * pow(2.0, lfo * octaves)
    Example: fc = 1000 * pow(2, tri * 1.5)  → 354–2828Hz (±1.5 octave)

One-sided mapping (0–1 range parameter):
    param = base + (lfo * 0.5 + 0.5) * range
    Example: mix = (tri * 0.5 + 0.5) * 0.6  → 0–0.6
```

### Adding Stereo Width with ModulationBus

```cpp
// ModulationBus is a stereo LFO phase routing utility
for (int ch = 0; ch < 2; ++ch)
{
    // tri() is phase-inverted for ch=1 (-0.75×) → increases stereo width
    double mod = ModulationBus::tri(lfo, ch, /*depth01=*/0.8);
    float fc = 800.0f + (float)mod * 600.0f;
    svf.setCutoffHz(fc);
    io[ch][i] = svf.process(ch, io[ch][i], 0);
}
```

---

<a id="recipe-5"></a>
## Recipe 5: Phaser + Tremolo Combined Effect (L3)

Connect two modulation effects in series.

```cpp
#include "include/patina.h"

constexpr double kSr = 48000.0;
constexpr int kCh = 2;

AnalogLfo   lfo;
PhaserStage phaser;
TremoloVCA  tremolo;

void prepare() {
    patina::ProcessSpec spec{kSr, 256, kCh};
    lfo.prepare(kCh, kSr);
    phaser.prepare(kCh, kSr);
    tremolo.prepare(kCh, kSr);

    lfo.setRateHz(1.5);   // Shared LFO
}

float process(int ch, float x)
{
    // Get current LFO values (call stepAll() externally)
    float tri = lfo.getTri(ch);
    float sin = lfo.getSinLike(ch);

    // Stage 1: Phaser (triangle LFO → allpass modulation)
    PhaserStage::Params pp;
    pp.lfoValue     = tri;         // -1 to +1
    pp.depth        = 0.6f;
    pp.feedback     = 0.5f;        // 0 to 0.95
    pp.centerFreqHz = 800.0f;
    pp.freqSpreadHz = 600.0f;
    pp.numStages    = 6;           // Even only (2–12)
    pp.temperature  = 25.0f;
    float phased = phaser.process(ch, x, pp);

    // Stage 2: Tremolo (sine LFO → amplitude modulation)
    TremoloVCA::Params tp;
    tp.depth    = 0.4f;
    tp.lfoValue = sin;             // Sine is smoother
    tp.mode     = 1;               // 0=Bias, 1=Optical, 2=VCA
    tp.stereoPhaseInvert = true;   // Invert ch1 → stereo width
    float out = tremolo.process(ch, phased, tp);

    return out;
}
```

**Notes:**
- Use both `getTri()` and `getSinLike()` from the same `AnalogLfo` for two waveforms
- `PhaserStage`'s `numStages` must be even (avoids stereo phase cancellation)
- `TremoloVCA`'s Optical mode is an asymmetric AR envelope (hardware: attack 1ms / release 10ms)

---

<a id="recipe-6"></a>
## Recipe 6: Hand-built BBD Delay (L3)

Build a BBD delay by wiring L3 modules directly, without using `BbdDelayEngine`.

> ```bash
> c++ -std=c++17 -O2 -I. your_file.cpp -o your_app
> ```

```cpp
#include "include/patina.h"
#include <vector>
#include <random>

constexpr double kSr = 48000.0;
constexpr int kCh = 2;

// ── L3 Modules ──
CompanderModule  compressor;
CompanderModule  expander;
BbdSampler       bbdSampler;
BbdStageEmulator bbdEmu;
ToneFilter       tone;

// ── Delay buffer (ring buffer) ──
constexpr int kMaxDelay = 48000;  // 1 second
std::vector<float> delayBuf[kCh];
int writePos = 0;

// ── BBD random source ──
std::minstd_rand rng(12345);
std::normal_distribution<double> normalDist(0.0, 1.0);

// ── Feedback storage ──
float fb[kCh] = {};

void prepare()
{
    compressor.prepare(kCh, kSr);
    expander.prepare(kCh, kSr);
    bbdSampler.prepare(kCh, kSr);
    bbdEmu.prepare(kCh, kSr);
    tone.prepare(kSr, kCh, 256);
    tone.setDefaultCutoff(3000.0f);

    for (int ch = 0; ch < kCh; ++ch)
        delayBuf[ch].resize(kMaxDelay, 0.0f);
}

struct BbdParams {
    float delayMs    = 200.0f;   // 10–800ms
    float feedback   = 0.45f;    // 0–0.95
    float compAmount = 0.7f;     // Compander amount
    float mix        = 0.5f;     // Dry/Wet
    int   stages     = 4096;     // BBD stages
    float supplyV    = 9.0f;
};

float process(int ch, float dry, const BbdParams& p)
{
    // 1. Compressor (NE570 pre-conditioning)
    float compressed = compressor.processCompress(ch, dry, p.compAmount);

    // 2. Write to ring buffer (add feedback)
    delayBuf[ch][writePos] = compressed + fb[ch] * p.feedback;

    // 3. Read from ring buffer
    int delaySamples = (int)(p.delayMs * 0.001 * kSr);
    int readIdx = writePos - delaySamples;
    if (readIdx < 0) readIdx += kMaxDelay;
    float delayed = delayBuf[ch][readIdx];

    // 4. BBD S&H (sample & hold quantization)
    double step = (double)kMaxDelay / delaySamples;
    delayed = bbdSampler.processSample(
        ch, delayed, step,
        /*emulateBBD=*/true, /*enableAging=*/false,
        /*clockJitterStd=*/0.001,
        rng, normalDist);

    // 5. BBD stage frequency response
    std::vector<float> frame = {delayed, delayed};  // stereo frame
    bbdEmu.process(frame, step, p.stages, p.supplyV,
                   /*enableAging=*/false, /*ageYears=*/0.0);
    delayed = frame[ch];

    // 6. Expander (compander restore)
    float expanded = expander.processExpand(ch, delayed, p.compAmount);

    // 7. Tone filter
    expanded = tone.processSample(ch, expanded);

    // 8. Store feedback
    fb[ch] = expanded;

    // 9. Dry/Wet mix
    return DryWetMixer::equalPowerMixVolt(dry, expanded, p.mix, 1.0f);
}

// Advance writePos in the main loop
void advanceWritePos() {
    writePos = (writePos + 1) % kMaxDelay;
}
```

**Signal Flow Diagram:**
```
Input
  ↓
CompanderModule::processCompress()   ← NE570 2:1 圧縮
  ↓
DelayBuffer[write] ← + feedback
    ↓
DelayBuffer[read]                    ← delayed by delay time
    ↓
BbdSampler::processSample()          ← S&H quantization + clock jitter
    ↓
BbdStageEmulator::process()          ← BBD tone coloring
    ↓
CompanderModule::processExpand()     ← NE570 1:2 expansion
    ↓
ToneFilter::processSample()          ← RC LPF tone control
    ↓                   ↓
DryWetMixer      → feedback storage
    ↓
Output
```

---

<a id="recipe-7"></a>
## Recipe 7: Compressor Chain — Photo → FET Cascade (L3)

Two-stage compressor: smooth leveling with Photo → fast peak control with FET.

```cpp
#include "include/patina.h"

constexpr double kSr = 48000.0;
constexpr int kCh = 2;

PhotoCompressor photo;
FetCompressor   fet;

void prepare() {
    patina::ProcessSpec spec{kSr, 256, kCh};
    photo.prepare(spec);
    fet.prepare(spec);
}

float processCompChain(int ch, float x)
{
    // Stage 1: Photo (T4B) — smooth leveling
    PhotoCompressor::Params pp;
    pp.peakReduction = 0.5f;    // Amount of gain reduction
    pp.outputGain    = 0.5f;    // Makeup gain
    pp.mode          = 0;       // 0=Compress (≈3:1)
    pp.mix           = 1.0f;    // 100% wet
    float leveled = photo.process(ch, x, pp);

    // Stage 2: FET (2N5457) — peak control
    FetCompressor::Params fp;
    fp.inputGain  = 0.4f;      // Modest (since after Photo)
    fp.outputGain = 0.5f;
    fp.attack     = 0.2f;      // Fast attack (≈100µs)
    fp.release    = 0.4f;      // Medium release (≈300ms)
    fp.ratio      = 1;         // 1=8:1
    fp.mix        = 0.7f;      // Parallel comp
    float controlled = fet.process(ch, leveled, fp);

    return controlled;
}
```

**Why this order?**
- Photo (T4B) has slow attack 7ms / release 500ms — good for musical leveling
- FET (2N5457) has fast attack 20µs — good for taming transients
- Photo → FET: first level overall, then control peaks

### Checking Gain Reduction Amount

```cpp
float grPhoto = photo.getGainReductionDb(ch);  // Example: -4.2 dB
float grFet   = fet.getGainReductionDb(ch);    // Example: -2.1 dB
// Total: ≈ -6.3 dB gain reduction
```

---

<a id="recipe-8"></a>
## Recipe 8: Tape Saturation + Transformer for “Mastering Glue” (L3)

Mastering-grade saturation combining tape and transformer saturation.

```cpp
#include "include/patina.h"

constexpr double kSr = 48000.0;
constexpr int kCh = 2;

TapeSaturation   tape;
TransformerModel transformer;

void prepare() {
    patina::ProcessSpec spec{kSr, 256, kCh};
    tape.prepare(spec);
    transformer.prepare(spec);
}

float processMasterGlue(int ch, float x)
{
    // Stage 1: Tape saturation (15ips half-inch)
    TapeSaturation::Params tp;
    tp.inputGain    = 0.3f;     // Modest input (for mastering)
    tp.saturation   = 0.25f;    // Light saturation
    tp.biasAmount   = 0.6f;     // Bias optimization
    tp.tapeSpeed    = 1.0f;     // 15ips
    tp.wowFlutter   = 0.05f;    // Small amount of W&F (texture)
    tp.enableHeadBump = true;   // LF head bump (≈60Hz)
    tp.enableHfRolloff = true;  // HF self-erasure
    tp.headWear     = 0.1f;     // Slightly used
    tp.tapeAge      = 0.0f;     // New tape
    float taped = tape.process(ch, x, tp);

    // Stage 2: Console transformer (British)
    TransformerModel::Params xp;
    xp.driveDb      = 0.0f;     // Unity
    xp.saturation   = 0.2f;     // Light saturation
    xp.enableLfBoost  = true;   // Coupling around 80Hz
    xp.enableHfRolloff = true;  // 18kHz rolloff
    xp.dcBias       = 0.0f;     // No DC offset
    xp.temperature  = 40.0f;    // Operating temperature
    float transformed = transformer.process(ch, taped, xp);

    return transformed;
}
```

**Resulting Character:**
- Tape: Adds even harmonics + LF thickness from head bump + natural HF rolloff
- Transformer: More even harmonics from core saturation + subtle peaks from winding resonance + LF coupling

---

<a id="recipe-9"></a>
## Recipe 9: Opto Comp VCA from L2 Parts (L2)

Build an opto VCA with program-dependent release at the component level using `PhotocellPrimitive` + `EnvelopeFollower`.

```cpp
#include "include/patina.h"
#include <cmath>

constexpr double kSr = 48000.0;

PhotocellPrimitive cell(PhotocellPrimitive::T4B());
// T4B: attack=10ms, release=60–5000ms (memory-modulated)
// VTL5C3: attack=5ms, release=30–2000ms (faster)

void prepare() {
    cell.prepare(kSr);
    cell.reset();
}

float processOptoVCA(float input)
{
    // 1. Sidechain: detect input level (simple peak)
    float scLevel = std::abs(input);

    // 2. Photocell processing: sidechain → attenuation amount
    //    Return: 0.0 (no attenuation) to 1.0 (max attenuation)
    double attenuation = cell.process(scLevel);

    // 3. VCA: gain = 1 - attenuation
    float gain = (float)(1.0 - attenuation);

    // 4. Output
    return input * gain;
}
```

**PhotocellPrimitive “Memory Effect” Explanation:**

```
Sustained compression → historyAccum builds up
    → Release time extends from 60ms → several hundred ms → up to 5000ms
    → After long compression, release is slow

Short transients only → historyAccum stays low
    → Release is fast, around 60ms

This is the origin of the classic opto compressor’s “program-dependent release”
```

### Comparison of Two Cell Types

```cpp
PhotocellPrimitive t4b(PhotocellPrimitive::T4B());
PhotocellPrimitive vtl(PhotocellPrimitive::VTL5C3());

// T4B:    Attack 10ms, Release 60–5000ms → vintage, smooth
// VTL5C3: Attack  5ms, Release 30–2000ms → modern, tight
```

---

<a id="parameter-routing"></a>
## Parameter Routing Quick Reference

### LFO → Module Parameter Connections

| Target | Parameter Name | LFO Output | Mapping |
|---|---|---|---|
| StateVariableFilter | `cutoffHz` | `getTri(ch)` | `center + tri * depth` |
| LadderFilter | `cutoffHz` | `getTri(ch)` | `center * pow(2, tri * oct)` |
| PhaserStage | `lfoValue` | `getTri(ch)` | **Direct assignment** (-1 to +1) |
| TremoloVCA | `lfoValue` | `getSinLike(ch)` | **Direct assignment** (-1 to +1) |
| BbdClock | `triVal` | `getTri(ch)` | `BbdClock::stepWithClock()` |
| DiodeClipper | `drive` | `getSinLike(ch)` | `base + (sin*0.5+0.5) * range` |

### Sidechain → Dynamics Connection Methods

| Source | Detection | Target | Notes |
|---|---|---|---|
| Input signal | `abs(x)` | PhotocellPrimitive | Peak detection is sufficient |
| Input signal | EnvelopeFollower | FetCompressor | RMS detection recommended |
| External sidechain | Other channel | NoiseGate | Remove LF with `sidechainHpHz` |

### Temperature Parameter Effects

| Module | Effect | Typical Value |
|---|---|---|
| DiodePrimitive | Vf drop (−2mV/°C) → earlier clip | 25–50°C |
| OTA_Primitive | gm change (−0.3%/°C) → fc drift | 25–45°C |
| BJT_Primitive | Vbe drift (−0.2%/°C) → fc shift | 25–45°C |
| JFET_Primitive | Vp drift → fc change | 25–40°C |
| TransformerPrimitive | Permeability change → saturation characteristic change | 30–50°C |
| TubePreamp | supply ripple + emission loss | 25–60°C |

---

<a id="troubleshooting"></a>
## トラブルシューティング

### Q: BBD フィードバックが暴走する
## Troubleshooting

### Q: The filter self-oscillates

```cpp
// ✗ Resonance is too high
ladder.setResonance(1.0f);  // → Self-oscillation
ladder.process(ch, x, 0.0f);

// ✓ Safe range
ladder.setResonance(0.85f);
ladder.process(ch, x, 0.2f);  // When drive > 0, resonance naturally damps
```

> When `drive > 0` in LadderFilter, resonance is automatically damped according to input level (same behavior as transistor saturation in hardware).

### Q: BBD feedback runs away
```cpp
// ✗ Feedback + compander interaction
params.feedback   = 0.95f;
params.compAmount = 1.0f;   // Compression increases apparent gain

// ✓ Make feedback inversely proportional to compander amount
float safeFb = 0.8f * (1.0f - compAmount * 0.3f);
```

### Q: Forgot to call prepare()
```
// Symptom: Output is 0, NaN, or abnormally large
// Cause: α coefficient not calculated
// Fix: Always call prepare(sampleRate) first

// Checklist:
// ☐ TubeTriode.prepare(sr)
// ☐ RC_Element.prepare(sr)
// ☐ TransformerPrimitive.prepare(sr)
// ☐ PhotocellPrimitive.prepare(sr)
// ☐ All L3 modules .prepare(spec) or .prepare(numCh, sr)
```

### Q: Stereo image collapses to mono in multichannel
```cpp
// ✗ Creating OTA/BJT with the same seed
OTA_Primitive ota_L(OTA_Primitive::LM13700(), 42);
OTA_Primitive ota_R(OTA_Primitive::LM13700(), 42);  // ← Same mismatch

// ✓ Create subtle differences with different seeds
OTA_Primitive ota_L(OTA_Primitive::LM13700(), 42);
OTA_Primitive ota_R(OTA_Primitive::LM13700(), 43);  // ← Different mismatch
// → Small gm differences create natural stereo width
```

### Q: Linker error (undefined symbol)
```
// All modules are header-only
c++ -std=c++17 -O2 -I. your_file.cpp -o your_app
```

### Q: CPU load spikes due to denormals

The L4 engine automatically applies `ScopedDenormalDisable` inside `processBlock`, so no extra handling is needed when using the engine.

If using L2/L3 directly, insert a guard manually:
```cpp
#include "dsp/core/DenormalGuard.h"

void myProcessBlock(float** io, int nCh, int nSamples) {
    patina::ScopedDenormalDisable guard;  // Enable FTZ/DAZ
    for (int i = 0; i < nSamples; ++i)
        for (int ch = 0; ch < nCh; ++ch)
            io[ch][i] = myFilter.process(ch, io[ch][i]);
}   // ← Automatically restored
```

### Q: NaN propagates and mutes audio

If the filter's internal state is contaminated with NaN, all subsequent output becomes NaN.
The L4 engine automatically applies `FastMath::sanitize()` to input and filter state.

If using L2/L3 directly, check the output:

```cpp
float y = svf.process(ch, x, filterType);
y = FastMath::sanitize(y);  // NaN/Inf → 0.0f
```
float y = svf.process(ch, x, filterType);
y = FastMath::sanitize(y);  // NaN/Inf → 0.0f
```

---

<a id="recipe-10"></a>
## Recipe 10: M/S Iron — Transformer Method (L3)

**Class used:** `MidSideIron`  
**L2 dependency:** `TransformerPrimitive × 2`

```
#include "dsp/circuits/modulation/MidSideIron.h"
```

This method constructs an M/S matrix through transformer cores. Magnetic saturation and hysteresis add even-order harmonics and LF thickness, resulting in a “fat, cohesive” console-like sound.

### Minimal configuration

```cpp
MidSideIron msIron;                       // Default: MidSideConsole() preset
msIron.prepare(spec.sampleRate);

MidSideIron::Params p;
p.satAmount   = 0.15f;   // Core saturation amount (0=clean, 0.15=default, 1.0=heavy)
p.temperature = 25.0f;   // Temperature → affects core permeability μ(T)

// Per-sample processing
for (int i = 0; i < numSamples; ++i) {
    auto [mid, side] = msIron.encode(L[i], R[i], p);

    mid  *= midGain;    // Process Mid/Side independently
    side *= sideGain;

    auto [outL, outR] = msIron.decode(mid, side, p);
    L[i] = outL;
    R[i] = outR;
}
```

### One-call round-trip

```cpp
// Combine encode → gain → decode in one call
for (int i = 0; i < numSamples; ++i) {
    auto [outL, outR] = msIron.process(L[i], R[i], midGain, sideGain, p);
    L[i] = outL;
    R[i] = outR;
}
```

### Transformer preset switching

```cpp
// Default: MidSideConsole() — Neve-style 1:1, core saturation level 1.2
MidSideIron ms;                                            // Default

MidSideIron msBritish(TransformerPrimitive::BritishConsole());  // More console-like
MidSideIron msLundahl(TransformerPrimitive::Lundahl1538());     // High-resolution studio
MidSideIron msAmerican(TransformerPrimitive::AmericanConsole()); // Tighter punch
```

### Instance structure

```
xfmrEnc[0] = encode Mid path
xfmrEnc[1] = encode Side path
xfmrDec[0] = decode L path
xfmrDec[1] = decode R path
```

Encode and decode each have separate sets of 4 transformer instances.  
As in hardware, the magnetic state is accumulated independently in the encode and decode stages, so calling `encode → decode` in sequence does not contaminate state.  
Whether using `process()` or calling `encode/decode` individually, operation is equivalent and safe.

---

<a id="recipe-11"></a>
## Recipe 11: M/S Precision — Op-Amp Method (L3)

**Class used:** `MidSidePrecision`  
**L2 dependency:** `OpAmpPrimitive × 2`

```
#include "dsp/circuits/modulation/MidSidePrecision.h"
```

This is an active method for constructing an M/S matrix using an inverting summing op-amp circuit.  
It offers higher transparency than the transformer method, and the choice of IC allows you to adjust slew rate, bandwidth, and saturation character.  
This is the highest-precision M/S technique used in high-end mastering consoles.

### Minimal configuration

```cpp
// Default: TL072CP (JFET input, 13V/µs slew)
MidSidePrecision msPrec;
msPrec.prepare(spec.sampleRate);

MidSidePrecision::Params p;
p.highVoltage = true;   // true=18V rails (wide headroom), false=9V (earlier saturation)

for (int i = 0; i < numSamples; ++i) {
    auto [mid, side] = msPrec.encode(L[i], R[i], p);

    mid  *= midGain;
    side *= sideGain;

    auto [outL, outR] = msPrec.decode(mid, side, p);
    L[i] = outL;
    R[i] = outR;
}
```

### IC preset comparison and use cases

```cpp
// Transparent: mastering grade
MidSidePrecision msClean(OpAmpPrimitive::LM4562());   // THD+N=-140dB, ultra-wide bandwidth 55MHz
MidSidePrecision msHifi (OpAmpPrimitive::OPA2134());  // Hi-Fi clean, high headroom

// Console: mixing work
MidSidePrecision msConsole(OpAmpPrimitive::NE5532()); // Punch, density, console character
MidSidePrecision msBright (OpAmpPrimitive::TL072CP()); // Bright, open (default)

// Vintage: characterful
MidSidePrecision msVintage(OpAmpPrimitive::JRC4558D()); // Dark, warm, low slew rate
MidSidePrecision msRaw    (OpAmpPrimitive::LM741());    // 1968 original IC, raw roughness
```

### Comparison with MidSideIron

| Aspect | MidSideIron | MidSidePrecision |
|------|-------------|-----------------|
| L2 Part | `TransformerPrimitive` | `OpAmpPrimitive` |
| Character | Core warmth, even-order harmonics | IC characteristics (slew rate, bandwidth) |
| Transparency | Medium (intentional coloration) | High (IC-dependent, nearly transparent) |
| Best for | Vintage/analog feel | Clean processing, precision |
| Custom parts | Design with `TransformerPrimitive::Spec` | Design with `OpAmpPrimitive::Spec` |

### Block processing pattern (stereo buffer)

```cpp
void processMidSide(float* const* io, int numCh, int numSamples,
                    float midGain, float sideGain)
{
    if (numCh < 2) return;

    MidSidePrecision::Params p{ .highVoltage = true };

    for (int i = 0; i < numSamples; ++i)
    {
        auto [mid, side] = msPrec.encode(io[0][i], io[1][i], p);
        mid  *= midGain;
        side *= sideGain;
        auto [outL, outR] = msPrec.decode(mid, side, p);
        io[0][i] = outL;
        io[1][i] = outR;
    }
}
```

---

<a id="recipe-12"></a>
## Recipe 12: Passive LC Filter — Inductor Harmonics (L3)

**Class used:** `PassiveLCFilter`  
**L2 dependency:** `InductorPrimitive × 1` + `RC_Element × 1`

```
#include "dsp/circuits/filters/PassiveLCFilter.h"
```

Models passive EQs in the style of classic units and wah pedal resonant filters.  
Core saturation in the inductor adds harmonics at high input, and DCR insertion loss gives an analog feel.

### Basic pattern

```cpp
PassiveLCFilter filt(InductorPrimitive::HaloInductor());
filt.prepare(2, 48000.0);
filt.setCutoffHz(800.0f);
filt.setResonance(0.6f);

PassiveLCFilter::Params p;
p.cutoffHz = 800.0f;
p.resonance = 0.6f;
p.drive = 0.3f;         // Inductor saturation (harmonic amount)
p.filterType = PassiveLCFilter::BPF;

for (int i = 0; i < numSamples; ++i)
    io[0][i] = filt.process(0, io[0][i], p);
```

### Filter type selection

| Type | Use | Description |
|--------|------|------|
| `LPF` | Dark EQ, low-pass | 2nd order -12dB/oct low-pass |
| `HPF` | High-pass, rumble removal | 2nd order -12dB/oct high-pass |
| `BPF` | Wah, resonant peak | Band-pass (emphasizes resonant frequency) |
| `Notch` | Notch filter | Band rejection (hum removal, etc.) |

### Inductor presets and sound

```cpp
// Classic style — warm boost with Halo toroid
PassiveLCFilter pultec(InductorPrimitive::HaloInductor());

// Wah pedal — narrow resonant peak with Fasel inductor
PassiveLCFilter wah(InductorPrimitive::WahInductor());

// Air core — no saturation, linear LC filter
PassiveLCFilter linear(InductorPrimitive::AirCore());
```

---

<a id="recipe-13"></a>
## Recipe 13: Push-Pull Power Stage — Tube Amp Output Stage (L3)

**Class used:** `PushPullPowerStage`  
**L2 dependency:** `PowerPentode × 2` + `TransformerPrimitive × 1`

```
#include "dsp/circuits/saturation/PushPullPowerStage.h"
```

Models the output stage of guitar/bass amps and hi-fi tube power amps.  
Simulates matched output tube pairs, phase inverter circuits, output transformers, and negative feedback.

### Topology presets

```cpp
auto marshall = PushPullPowerStage::Marshall50W();   // EL34 — British crunch
auto fender   = PushPullPowerStage::FenderTwin();    // 6L6GC — Clean headroom
auto vox      = PushPullPowerStage::VoxAC30();       // EL84 — Chimey, near class A
auto hifi     = PushPullPowerStage::HiFi_KT88();     // KT88 — Audiophile grade
auto deluxe   = PushPullPowerStage::FenderDeluxe();  // 6V6GT — Sweet breakup
```

### Basic pattern

```cpp
auto amp = PushPullPowerStage::Marshall50W();
amp.prepare(2, 48000.0);

PushPullPowerStage::Params p;
p.inputGainDb      = 12.0f;   // Input gain from preamp
p.bias             = 0.65f;   // Normal class AB
p.negativeFeedback = 0.3f;    // Moderate NFB
p.presenceHz       = 5000.0f; // Presence control
p.sagAmount        = 0.6f;    // Vintage rectifier sag
p.outputLevel      = 0.7f;

for (int i = 0; i < numSamples; ++i)
    io[0][i] = amp.process(0, io[0][i], p);
```

### Bias setting and character

| bias value | Class | Character |
|---------|--------|-------------|
| 0.0–0.3 | class B (cold) | Strong crossover distortion, gritty |
| 0.5–0.7 | class AB (standard) | Well-balanced, standard for guitar amps |
| 0.8–1.0 | class A (hot) | Even-order harmonics, rich, Vox-like |

### Example: Connecting preamp to power stage

```cpp
TubePreamp preamp;
preamp.prepare(2, 48000.0);

auto powerStage = PushPullPowerStage::Marshall50W();
powerStage.prepare(2, 48000.0);

TubePreamp::Params preP{ .drive = 0.7f, .bias = 0.5f, .outputLevel = 0.8f };
PushPullPowerStage::Params powP{ .inputGainDb = 6.0f, .bias = 0.65f,
                                  .negativeFeedback = 0.3f, .sagAmount = 0.5f };

for (int i = 0; i < numSamples; ++i) {
    float v = preamp.process(0, io[0][i], preP);
    io[0][i] = powerStage.process(0, v, powP);
}
```