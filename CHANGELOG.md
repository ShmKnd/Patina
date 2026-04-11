# CHANGELOG

## 2026-04-10 — Documentation Audit, SIMD Experiment & Revert

### Docs: API_REFERENCE renumbering and missing modules (`4f65248`)

- Fixed 5 duplicate section numbers in TOC + body (§16, §27, §46, §51, §52)
- Renumbered all affected L3/L4/Config sections sequentially (19 → 83)
- Added 8 previously missing module entries: `FrontEnd`, `BbdClock`, `BbdNoise`, `BbdTimeController`, `DynamicsSuite`, `DuckingMixer`, `GainUtils`, `EnvelopeGeneratorEngine`
- Added 6 missing files to `examples/README.md`: `example_vco.cpp`, `example_ringmod.cpp`, `example_envelope_generator.cpp`, `example_eq_engine.cpp`, `example_limiter_engine.cpp`, `example_filter_engine.cpp`

**Changed files:** `docs/API_REFERENCE.md`, `examples/README.md`

---

### Revert: SIMD/NEON-accelerated processBlock (`92ae3c8`)

Reverted SIMD/NEON optimization for `StateVariableFilter` and `TransformerPrimitive`.  
No measurable throughput benefit on Intel Mac (SSE4.1 path) due to architectural constraints; code complexity not justified.

---

### Perf: SIMD/NEON-accelerated processBlock for SVF and TransformerPrimitive (`ebd479e`) *(subsequently reverted)*

- `StateVariableFilter::processBlock()`: NEON (AArch64) and SSE4.1 (x86-64) 4-wide vectorised paths
- `TransformerPrimitive::processBlock()`: same SIMD dispatch pattern
- Runtime guard via `__ARM_NEON` / `__SSE4_1__` macros; scalar fallback retained

---

## 2026-04-09 — Oversampler Bug Fixes

### Fix: MultistageOversampler x4 downsample stage propagation (`32ef5f8`)

- Processed data was not propagating through intermediate downsampling stages in 4x mode
- Each stage now correctly receives the output of the previous stage

### Fix: MultistageOversampler::process() DSP callback result discarded (`0c38445`)

- The DSP callback's processed buffer result was silently discarded; output was unprocessed signal
- Fixed: callback result now propagated to the output buffer correctly

### Fix: Oversampler — replace PolyphaseFilter decimator with FIRDecimator, fix -6dB loss (`6dc270c`)

- `PolyphaseFilter`-based decimator replaced with `FIRDecimator` for correct reconstruction
- Eliminated -6dB level loss in the downsample path caused by incorrect accumulation scaling

---

## 2026-04-08 — MidSide Matrix Circuits & Encoding Fix

### Fix: Source file encoding changed to UTF-8-BOM (`e75c557`)

### New: MidSideIron and MidSidePrecision M/S matrix circuits (`eb558bf`)

Two new L3 circuit modules added to `dsp/circuits/modulation/`:

| Module | Description |
|--------|-------------|
| `MidSideIron` | Passive transformer-based M/S encode/decode (Neve-style 1:1) |
| `MidSidePrecision` | Active op-amp M/S encode/decode (LM4562-based) |

- Both support per-sample encode/decode with analog coloration
- `patina.h` updated with new includes

**Changed files:** `dsp/circuits/modulation/MidSideIron.h` (new), `dsp/circuits/modulation/MidSidePrecision.h` (new), `include/patina.h`

---

## 2026-04-09 — New L2 Primitives & L3 Circuits

### New: Analog Component Primitives (L2)

**Added files:**

| File | Description |
|------|-------------|
| `dsp/parts/InductorPrimitive.h` | Choke coil / inductor primitive (iron/ferrite/air core) |
| `dsp/parts/PowerPentode.h` | Power pentode (beam tetrode) for push-pull amplifiers |
| `dsp/parts/VactrolPrimitive.h` | Vactrol (LED + LDR) optocoupler primitive |

**InductorPrimitive** — Physical choke coil model:
- Core saturation, hysteresis B-H curve, parasitic capacitance resonance
- Self-resonant frequency (SRF) calculation, Q factor modeling
- Temperature-dependent permeability
- Presets: `HaloInductor`, `WahInductor`, `PowerChoke`, `AirCore`

**PowerPentode** — Output tube for power amplifiers:
- Pentode transfer function with asymmetric clipping
- Class AB crossover notch, grid blocking, screen sag
- Ultralinear feedback, thermal compression
- Presets: `EL34`, `_6L6GC`, `KT88`, `EL84`, `_6V6GT`

**VactrolPrimitive** — LED + LDR optocoupler:
- Asymmetric LED/LDR attack/release, memory effect (history-dependent)
- Nonlinear resistance curve, temperature dependence
- Convenience `applyAsAttenuator()` method
- Presets: `VTL5C3`, `VTL5C1`, `NSL32SR3`, `DIY_Custom`

### New: Circuit Modules (L3)

| File | Description |
|------|-------------|
| `dsp/circuits/filters/PassiveLCFilter.h` | Passive LC filter (LPF/HPF/BPF/Notch) |
| `dsp/circuits/saturation/PushPullPowerStage.h` | Class AB push-pull tube power amplifier |

**PassiveLCFilter** — Passive inductor-capacitor filter:
- 4 filter types: LPF, HPF, BPF, Notch (2nd-order)
- Inductor core saturation adds analog harmonic character
- DCR insertion loss, temperature-dependent tuning
- Uses: `InductorPrimitive` + `RC_Element`

**PushPullPowerStage** — Tube power amplifier output stage:
- Matched pentode pair with phase inverter
- Output transformer saturation and coupling
- Adjustable class AB bias, negative feedback, presence control
- Power supply sag and screen grid interaction
- Topology presets: `Marshall50W`, `FenderTwin`, `VoxAC30`, `HiFi_KT88`, `FenderDeluxe`
- Uses: `PowerPentode` × 2 + `TransformerPrimitive`

---

## 2026-04-09 — Oversampler: Polyphase FIR Anti-Aliasing Filter

### New: Pure C++17 High-Quality Oversampler

**Added files:**

| File | Description |
|------|-------------|
| `dsp/core/Oversampler.h` | Polyphase FIR oversampler with Kaiser-windowed sinc filter |
| `tests/test_oversampler.cpp` | Unit tests and usage examples |

**Features:**
- **Zero JUCE dependency** — uses only C++17 standard library
- **Kaiser-windowed sinc FIR filter** — adjustable stopband attenuation (48–120 dB)
- **Efficient polyphase implementation** — optimized for real-time audio
- **Multiple quality modes** — Low/Medium/High/Ultra
- **2x, 4x, 8x, 16x oversampling** — single-stage or multi-stage
- **Per-channel processing** — arbitrary channel count support

**Classes:**
- `FIRDesigner` — Kaiser-windowed lowpass FIR filter coefficient generator
- `PolyphaseFilter<T>` — Efficient polyphase filter decomposition
- `Oversampler<T>` — Single-stage oversampler (2x/4x/8x/16x)
- `MultistageOversampler<T>` — Cascaded 2x stages for higher ratios

**Usage:**
```cpp
patina::Oversampler<float> os;
os.prepare(blockSize, numChannels, patina::OversamplingFactor::x4, patina::FilterQuality::High);

os.process(buffer, numSamples, [](auto& buf, int n) {
    // Process at oversampled rate
    for (int i = 0; i < n; ++i)
        buf.getWritePointer(0)[i] = saturate(buf.getReadPointer(0)[i]);
});
```

---

## 2026-04-06 — Phase 2: OpAmpPrimitive Instance Delegation

### Refactoring: Replace inline op-amp processing with `OpAmpPrimitive` method calls

**Changed files:**

| File | Changes |
|------|---------|
| `BbdFeedback.h` | Delegate Step 4a (slew-rate limiting) + Step 4c (saturation) to per-channel `OpAmpPrimitive` instances. Remove `opAmpDriftState` vector, add `opAmps_` vector. Support IC hot-swap via `setOpAmpOverrides()` |
| `InputBuffer.h` | Delegate inline slew-rate limiting to per-channel `OpAmpPrimitive::applySlewLimit()`. Remove `slewState` / `slewLimitPerSample` / `updateSlewLimit()`, add `slewAmps_` + `rebuildSlewInstances()` |
| `OpAmpConstants.h` | Add "migrated to `OpAmpPrimitive::Spec`" notes to IC-specific parameters. Classify and organize circuit-context-dependent constants |

**Design notes:**
- `asymNeg` (positive/negative asymmetry) now applies to BbdFeedback saturation (physically more accurate)
- InputBuffer saturation uses headroom-knee-based tanh model, different from OpAmpPrimitive; retained as-is
- OutputStage / CompanderModule / AnalogLfo use different domains/nonlinearities, excluded from this change

**Tests:** 634 test cases — all pass (no regressions)

---

## 2026-04-06 — Phase 1: OpAmpPrimitive Integration Refactoring

### Refactoring: Unify scattered op-amp type definitions into `OpAmpPrimitive::Spec`

**Changed files:**

| File | Changes |
|------|---------|
| `ModdingConfig.h` | Replace `OpAmpSpec` struct → `using OpAmpSpec = OpAmpPrimitive::Spec;`. Delegate `opAmpSpecs[]` table to OpAmpPrimitive presets. Add LM741 to `OpAmpType` enum |
| `Presets.h` | Simplify 6 boilerplate op-amp functions to 1-line table lookups. Remove `BbdFeedback.h` dependency. Add `lm741()` preset |
| `BbdFeedback.h` | Replace `OpAmpOverrides` struct → `using OpAmpOverrides = OpAmpPrimitive::Spec;`. `setOpAmpOverrides()` now accepts `OpAmpPrimitive::Spec` directly |
| `test_analog_presets.cpp` | Add direct `BbdFeedback.h` include |

**Design notes:**
- Audio output is identical (no-change refactoring)
- `noiseScale18V` / `noiseScale9V` reference `PartsConstants` directly (same behavior as before)
- `noiseMultiplier` → unified to `OpAmpPrimitive::Spec::noiseScale`
- `BbdFeedback::OpAmpOverrides` retained as backward-compatible alias

**Tests:** 634 test cases / 997,341 assertions — all pass

---

## 2026-04-06 — OpAmpPrimitive: Op-amp Abstraction + 6 IC Variants

### New: `dsp/parts/OpAmpPrimitive.h` (L2 Part Primitive)

Unified L2 primitive modeling op-amp IC slew rate, saturation, noise, bandwidth limiting, and manufacturing tolerance. Spec struct + `static constexpr` preset pattern.

**6 preset ICs:**

| IC | Characteristics |
|----|----------------|
| `TL072CP()` | JFET-input dual. Bright, open. Standard for BBD/LFO circuits |
| `JRC4558D()` | Low slew rate → dark, warm tone. Classic overdrive pedal op-amp |
| `NE5532()` | Console-grade density. Forward midrange presence |
| `OPA2134()` | Hi-Fi clean. Wide bandwidth, low distortion |
| `LM4562()` | Ultra-low noise, ultra-wide bandwidth. Studio transparency |
| `LM741()` | Original general-purpose IC. Narrow bandwidth, high offset — vintage fuzz "roughness" |

**Modeled properties:**
- Slew-rate limiting (`applySlewLimit`)
- Soft saturation — positive/negative asymmetry, supply-voltage dependent (`saturate`)
- GBW-based bandwidth limiting (`bandwidthHz`)
- Input-referred noise voltage density (`inputNoiseVrms`)
- Full-power bandwidth (`fullPowerBandwidthHz`)
- Offset voltage temperature drift (`offsetVoltage`)
- GBW manufacturing tolerance (`gbwMismatch`, seed-based)

### Tests

- `test_opamp_primitive.cpp` added: 22 TEST_CASE / 1,097 assertions
- Full suite: 634 TEST_CASE / 997,341 assertions (no regressions)

### Changed files

| File | Changes |
|------|---------|
| `dsp/parts/OpAmpPrimitive.h` | **New** — op-amp L2 primitive |
| `include/patina.h` | Add `OpAmpPrimitive.h` include |
| `tests/test_opamp_primitive.cpp` | **New** — 22 test cases |
| `tests/CMakeLists.txt` | Register `test_opamp_primitive.cpp` |

---

## 2026-04-06 — Test CMakeLists.txt Fix: Register Missing Tests & Remove Absent .cpp References

### Fix: Register 6 missing test files in `add_executable`

The following test files were missing from the `Patina_tests` target in `tests/CMakeLists.txt`:

- `test_analog_vco.cpp`
- `test_audio_transformer.cpp`
- `test_envelope_generator.cpp`
- `test_limiter_engine.cpp`
- `test_ring_modulator.cpp`
- `test_wave_folder.cpp`

### Fix: Remove references to non-existent .cpp files

`CompanderModule.cpp` / `BbdStageEmulator.cpp` were migrated to header-only and no longer exist in `dsp/circuits/`. Removed `DSP_SOURCES` variable entirely.

### Changed files

| File | Changes |
|------|---------|
| `tests/CMakeLists.txt` | Add 6 tests, remove `DSP_SOURCES` |
| `README.md` | Update header-only section, remove `.cpp` references from repo structure |

---

## 2026-04-06 — Robustness Hardening (P0–P3): Denormal / NaN / UB / C API Safety

Systematic improvement of audio-thread safety and C API completeness.

### P0 — Denormal & NaN Protection

- **`dsp/core/DenormalGuard.h` (new)**: RAII denormal suppression guard `ScopedDenormalDisable`
  - x86: SSE FTZ+DAZ (`_mm_setcsr`), AArch64: FPCR FZ bit, ARM32: FPSCR FZ bit
  - Automatic restoration on scope exit
- **`dsp/core/FastMath.h`**: Add `sanitize(float/double)` — returns 0 for `!std::isfinite(x)`
- **All 11 engine `processBlock`**: Add `ScopedDenormalDisable` guard + `FastMath::sanitize()` on inputs
- **SVF / LadderFilter / DiodeLadderFilter**: Add `FastMath::sanitize()` to internal state variables to block NaN propagation

### P1 — Crash / UB Prevention

- **Filter circuits (SVF / Ladder / DiodeLadder)**: Add empty-vector guards for `process()` called before `prepare()`
- **C API `_process()` (all 7 functions)**: Add early return guard for `!input || !output || num_channels <= 0 || num_samples <= 0`
- **C API documentation**: Add notes on handle invalidation after `_destroy()` in `patina_c.h`
- **BbdDelayEngine**: Clamp `supplyVoltage` to `std::clamp(1.0, 48.0)`

### P2 — Malfunction Prevention

- **BbdDelayEngine**: Clamp `feedback` to `std::clamp(0.0f, 0.98f)` (prevent self-oscillation)
- **BbdDelayEngine**: Replace per-block `std::vector<float>` heap allocation with member variable `bbdFrameVec`

### P3 — Quality Improvements

- **All 11 engines**: Zero-fill excess output channels (`numChannels > currentSpec.numChannels`)
- **C API (all 7 Params structs)**: Add `int pedal_mode` field (`0`=outboard, `1`=pedal)
  - Default `0` in `_default_params()`, wired as `r.pedalMode = p.pedal_mode != 0` in `toCpp()`

### Changed files

| File | Changes |
|------|---------|
| `dsp/core/DenormalGuard.h` | **New** — RAII denormal suppression |
| `dsp/core/FastMath.h` | Add `sanitize()` |
| `include/patina.h` | Add `DenormalGuard.h` include |
| `dsp/engine/*.h` (all 11) | Denormal guard, NaN sanitize, excess channel zero-fill |
| `dsp/circuits/filters/StateVariableFilter.h` | Empty-vector guard + state sanitize |
| `dsp/circuits/filters/LadderFilter.h` | Empty-vector guard + state sanitize |
| `dsp/circuits/filters/DiodeLadderFilter.h` | Empty-vector guard + state sanitize |
| `bindings/c/patina_c.h` | Add `pedal_mode`, lifecycle documentation |
| `bindings/c/patina_c.cpp` | Wire `pedal_mode`, add NULL/size guards |

---

## 2026-04-06 — patina.h Completion / FilterEngine Drive Improvement / Test Reorganization

### Fix: Smooth drive onset in FilterEngine

In `FilterEngine::processDrive()`, add dry/wet crossfade for drive type processing (Tube / Diode / Wave / Tape) over the `amount 0→0.2` range. Prevents abrupt timbral changes when increasing drive.

- Unify all `case` branches from `return` → `break` for shared blend processing
- `blend = min(amount * 5, 1)` for linear fade-in over 0→0.2

### Fix: Add missing includes to patina.h

5 headers were missing from the aggregate header `include/patina.h`:

| File | Category |
|------|---------|
| `dsp/circuits/dynamics/VcaCompressor.h` | L3 Dynamics |
| `dsp/circuits/modulation/RingModulator.h` | L3 Modulation |
| `dsp/engine/EqEngine.h` | L4 Engine |
| `dsp/engine/LimiterEngine.h` | L4 Engine |
| `dsp/engine/EnvelopeGeneratorEngine.h` | L4 Engine |

### Change: Test CMakeLists.txt reorganization

- Introduce `DSP_SOURCES` variable for explicit `.cpp` source file management
- Reorganize some test file entries

---

## 2026-04-05 — FilterEngine Addition

### New: FilterEngine (L4 Engine)

Dual filter + triple drive integrated engine.

- **Filter types**: LPF / HPF / BPF (OTA-SVF) + Ladder (BJT 4-pole -24dB)
- **Slope selection**: -6dB / -12dB / -18dB / -24dB/oct (Ladder fixed at -24dB)
  - -6dB: ToneFilter 1-pole
  - -12dB: 1× SVF (2-pole)
  - -18dB: SVF + ToneFilter cascade (3-pole)
  - -24dB: 2× SVF cascade (4-pole)
- **Drive**: Tube / Diode / WaveFolder / Tape (3 slots)
- **Routing**: Serial (D1→F1→D2→F2→D3) / Parallel (D1→[F1|F2]→D3)
- **Gain compensation**: Auto-normalize for volume loss when cutting (up to ~8dB)
  - `Params::normalize` (bool, default `true`) toggles gain compensation ON/OFF
- **Pedal mode**: Add InputBuffer / OutputStage

---

## 2026-04-04 — M. Directory Restructuring (Layer Architecture Clarification)

Reflect the 4-layer architecture (Constants < Parts < Circuits < Engines) directly in the directory structure. Consolidate L3 circuit modules under `dsp/circuits/` subdirectories.

### Include path changes

All L3 circuit module include paths now use `dsp/circuits/` prefix:

| Old path | New path |
|----------|----------|
| `dsp/bbd/...` | `dsp/circuits/bbd/...` |
| `dsp/compander/...` | `dsp/circuits/compander/...` |
| `dsp/drive/...` | `dsp/circuits/drive/...` |
| `dsp/filters/...` | `dsp/circuits/filters/...` |
| etc. | etc. |

**Tests:** 536 test cases / 670,138 assertions — all pass (no diff before/after)

---

## 2026-04-04 — L. C / Rust Language Bindings

### New: C API (`bindings/c/`)

Opaque-handle-based FFI layer exposing all 7 engines to C.

### New: Rust Crate (`bindings/rust/`)

Safe Rust wrapper with `Drop` (auto-release), `Send`, and `Default` (Params).

---

## 2026-04-04 — K. Engine-Specific Example Code

### New: `examples/` directory

Complete working samples for all 7 engines. Each sample generates input signals, processes them, and writes WAV output. Zero external dependencies (no JUCE), standard C++17 only.

---

## 2026-04-04 — J. L4 Integrated Effect Engine Expansion

### New engines (6 classes added, 7 total)

| Engine | Signal path |
|--------|------------|
| **DriveEngine** | InputBuffer → DiodeClipper → ToneFilter → OutputStage → Dry/Wet |
| **ReverbEngine** | InputBuffer → [SpringReverb \| PlateReverb] → ToneFilter → OutputStage → Dry/Wet |
| **CompressorEngine** | InputBuffer → [NoiseGate] → [Opto \| FET \| Variable-Mu] → OutputStage → Dry/Wet |
| **ModulationEngine** | InputBuffer → [Phaser \| Tremolo \| Chorus] → StereoImage → OutputStage → Dry/Wet |
| **TapeMachineEngine** | InputBuffer → TapeSaturation → TransformerModel → ToneFilter → OutputStage → Dry/Wet |
| **ChannelStripEngine** | InputBuffer → NoiseGate → TubePreamp → SVF(EQ) → TransformerModel → OutputStage |

**Tests:** 536 test cases / 670,138 assertions — all pass

---

## 2026-04-04 — I. Test Fixes / Warning Removal / Parts Layer Tests

### Fixes

- **Phaser silence test**: `fabs(out) < 1e-6f` → `<= 1e-6f` to handle exact 0.0f output
- **BbdStageEmulator.cpp**: Comment out unused parameters, remove unused variables. `-Wall -Wextra` clean

### New: Parts layer primitive tests (`test_parts_primitives.cpp`)

46 test cases covering all 9 primitives.

**Tests:** 498 test cases / 653,758 assertions — all pass

---

## 2026-04-04 — H. Parts Comments Enrichment + Library Rename

### Rename: AnalogDSP_kit → Patina

- Project name, namespace (`analogdsp` → `patina`), CMake target (`AnalogDSP` → `Patina`),
  aggregate header (`analogdsp.h` → `patina.h`), CMake options (`ANALOGDSP_*` → `PATINA_*`)

### Trademark removal

Remove trademarked product names from code identifiers, file names, and comments. Replace with circuit-topology-based generic names:

| Old name | New name | Rationale |
|----------|----------|-----------|
| `MS20Filter` | `OtaSKFilter` | OTA Sallen-Key topology name |
| `Neve1073` | `BritishConsole` | British console-style transformer |
| `API2520` | `AmericanConsole` | American console-style transformer |
| `Studer_A800` | `HighSpeedDeck` | High-speed studio deck |
| `Ampex_ATR102` | `MasteringDeck` | Mastering deck |
| etc. | etc. | |

### Parts comment enrichment

Add historical/musical context comments to all 9 Parts primitive preset factories (describing era, circuit topology, and use cases without using trademarked names).

**Tests:** 452 test cases / 653,565 assertions — all pass (no logic changes)

---

## 2026-04-04 — G. 4-Layer Architecture Refactoring (Constants → Parts → Circuits → Engines)

### New: Part Primitives layer (`dsp/parts/`)

9 analog part primitives encapsulating real-device physics: temperature dependence, manufacturing tolerance, and nonlinearity.

### Refactoring: 14 modules layered

Extract hardcoded constants and inline physics models from existing circuit modules, delegate to part primitives.

**Design principles:**
- **Public API fully compatible**: All `Params` structs and `process()`/`processBlock()` signatures unchanged
- **Multi-channel**: Stateful primitives instantiated per channel (PhotocellPrimitive) or use spec + external state vectors (TubePreamp)
- **Zero regressions**: 452 tests / 653,565 assertions — all pass

---

## 2026-04-04 — F. Analog Filters + Dynamics Modules

### New filter modules

| Module | Description |
|--------|------------|
| `OtaSKFilter` | OTA Sallen-Key filter (2-pole, LPF/HPF/BPF) |
| `DiodeLadderFilter` | Diode ladder filter (3-pole, "squelchy" acid sound) |
| `AnalogAllPass` | 1st/2nd order analog allpass filter |

### New dynamics modules

| Module | Description |
|--------|------------|
| `PhotoCompressor` | Opto-coupled compressor (photocell + tube output stage) |
| `FetCompressor` | FET compressor (ultra-fast attack, 5 ratio presets) |
| `VariableMuCompressor` | Variable-mu tube compressor (remote-cutoff tube, push-pull) |

---

## 2026-04-03 — E. Power Supply & Environmental Modeling

### New modules

| Module | Description |
|--------|------------|
| `PowerSupplySag` | Tube rectifier power supply sag model |
| `BatterySag` | 9V battery dying-battery effect |
| `AdapterSag` | DC adapter power supply model (9V/18V, linear/switching/unregulated) |
| `CapacitorAging` | Capacitor aging simulation (ESR increase, capacitance loss) |

**Tests:** 384 test cases / 651,936 assertions — all pass

---

## Milestone 4 — Module Decomposition + Rename + Folder Reorganization

### PartsConstants split
- Split `PartsConstants.h` into 6 domain-specific files

### Folder relocation + class renames (11 classes)

| Old name | New name |
|----------|----------|
| DriveModule | DiodeClipper |
| TL072InputBuffer | InputBuffer |
| InputLPF | InputFilter |
| OutputModule | OutputStage |
| TL072LFO | AnalogLfo |
| ToneFilterBank | ToneFilter |
| DelayCore | BbdClock |
| FeedbackModule | BbdFeedback |
| FeedbackBus | BbdNoise |
| TimeStageController | BbdTimeController |

### Mixer decomposition
- `Mixer.h` (215 lines) → 3 independent modules: `DryWetMixer.h`, `DuckingMixer.h`, `GainUtils.h`

**Tests:** 323 test cases / 461,379 assertions — all pass

---

## 2026-03-27 — Repository Cleanup & Library Packaging

- Rename `dsp/core/JuceCompat.h` → `dsp/core/AudioCompat.h`
- Remove `examples/` directory (smoke tests and JUCE-dependent samples)
- Fix `include/patina.h` include paths

---

## 2026-03-27 — JUCE Dependency Removal & Standalone Build

- Replace all JUCE API calls with C++ standard equivalents
- `juce::jlimit` → `std::clamp`, `juce::AudioBuffer` → `patina::compat::AudioBuffer`, etc.
- Confirm standalone CMake build succeeds
