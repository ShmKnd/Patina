## Patina Examples

A collection of sample programs demonstrating usage across the 4-layer architecture (L1–L4).
Each sample runs with zero external dependencies (no JUCE required) and uses only standard C++17.

### Build Instructions

```bash
cd AnalogDSP_kit
# Example: DriveEngine sample (L4)
c++ -std=c++17 -O2 -I. examples/example_drive_engine.cpp -o example_drive
./example_drive
# → produces output_drive.wav

# Example: PassiveLCFilter (new L3)
c++ -std=c++17 -O2 -I. examples/example_passive_lc_filter.cpp -o example_lc
./example_lc
# → produces output_passive_lc_filter.wav

# Example: PushPullPowerStage (new L3)
c++ -std=c++17 -O2 -I. examples/example_push_pull_power_stage.cpp -o example_push_pull
./example_push_pull
# → produces output_push_pull_power_stage.wav

# Example: L1 constants sample (no WAV output, console output only)
c++ -std=c++17 -O2 -I. examples/example_l1_constants.cpp -o example_l1
./example_l1

# Example: L3 circuits sample
c++ -std=c++17 -O2 -I. examples/example_l3_circuits.cpp -o example_l3
./example_l3
```

## Samples

### L1 — Constants (physical constants & IC specs)

| File | Description |
|---|---|
| `example_l1_constants.cpp` | Display a list of frequencies, time constants, and thresholds derived from circuit constants. RC filter design examples. |

### L2 — Parts (analog primitives)

| File | Parts | Description |
|---|---|---|
| `example_l2_parts.cpp` | DiodePrimitive, TubeTriode, OTA, JFET, Transformer, Tape, BJT, RC, Photocell | Output WAVs demonstrating nonlinear behavior and temperature dependence of nine analog part models. **Hand-build a one-stage ladder filter using BJT+RC** |

### L3 — Circuits (circuit modules)

| File | Module | Description |
|---|---|---|
| `example_l3_circuits.cpp` | DiodeClipper, SVF, LadderFilter, TubePreamp, TapeSaturation, Phaser+LFO, **Compander+BBD** | Modular wiring of L3 circuit blocks. **Assemble a BBD delay by hand without using an Engine** |
| `example_wavefolder.cpp` | WaveFolder | Buchla 259-style wavefolder. Si/Ge diode comparison and varying folding stages |
| `example_vco.cpp` | AnalogVCO | BJT differential-pair current source + RC integrator VCO. Compare Saw / Pulse+PWM / Triangle with temperature drift |
| `example_ringmod.cpp` | RingModulator | 4‑diode bridge ring modulator. Si / Schottky / Ge comparison — carrier leakage and tonal character |
| `example_vco_ringmod.cpp` | AnalogVCO + RingModulator | VCO-generated carrier with ring modulator. Saw/Pulse/Tri × Si/Schottky/Ge |
| `example_passive_lc_filter.cpp` | **PassiveLCFilter** | Passive LC filter. Halo LPF sweep, Fasel wah BPF resonance, AirCore HPF+Notch. Inductor saturation comparison |
| `example_push_pull_power_stage.cpp` | **PushPullPowerStage** | Tube power amp output stage. Compare 4 topologies: Marshall / Fender / Vox / Deluxe, plus power sag comparison |
| `example_envelope_generator.cpp` | **EnvelopeGeneratorEngine** | Analog ADSR envelope. ADSR (RC curve) + AnalogVCO patch, AD percussive one-shot, auto-trigger mode |

### L4 — Engines (integrated effect engines)

| File | Engine | Description |
|---|---|---|
| `example_drive_engine.cpp` | DriveEngine | Overdrive using diode clippers. Compare Si / Schottky / Ge |
| `example_reverb_engine.cpp` | ReverbEngine | Spring & plate reverb comparisons |
| `example_compressor_engine.cpp` | CompressorEngine | Compare Photo / FET / Variable‑Mu compressors |
| `example_modulation_engine.cpp` | ModulationEngine | Phaser / Tremolo / Chorus comparisons |
| `example_tape_machine_engine.cpp` | TapeMachineEngine | Tape + transformer saturation |
| `example_channel_strip_engine.cpp` | ChannelStripEngine | Console channel strip |
| `example_bbd_delay_engine.cpp` | BbdDelayEngine | BBD analog delay + chorus modulation |
| `example_eq_engine.cpp` | EqEngine | 3‑band parametric EQ (Low Shelf / Mid Bell / High Shelf). OTA‑SVF based analog console EQ |
| `example_limiter_engine.cpp` | LimiterEngine | 3 limiter types: FET (1176 All‑Buttons), VCA (SSL/dbx ∞:1), Opto (LA‑2A style opto) |
| `example_filter_engine.cpp` | FilterEngine | Dual filter + triple drive. Serial/Parallel routing, 4 drive types, slope switching (-6 / -12 / -18 / -24 dB/oct) |

## API Patterns

### L1: Constants access
```cpp
#include "include/patina.h"
// Directly reference constants
double fc = PartsConstants::outputLpfFcHz;  // derived cutoff
double vf = PartsConstants::diode_forward_v; // diode Vf
```

### L2: Part models (per-sample)
```cpp
DiodePrimitive diode(DiodePrimitive::Si1N4148());
double clipped = diode.clip(input, 25.0);      // temperature-specified clipping

TubeTriode tube(TubeTriode::T12AX7());
tube.prepare(48000.0);
double out = TubeTriode::transferFunction(input, 1.0);
```

### L3: Circuit modules (initialize with ProcessSpec)
```cpp
patina::ProcessSpec spec{48000.0, 256, 2};

StateVariableFilter svf;
svf.prepare(spec);
svf.setCutoffHz(1000.0f);
svf.setResonance(0.7f);
float out = svf.process(channel, input, typeIndex);
```

### L4: Integrated engines (processBlock)
```cpp
patina::DriveEngine engine;
patina::ProcessSpec spec{48000.0, 256, 2};
engine.prepare(spec);

patina::DriveEngine::Params params;
params.drive = 0.8f;
params.tone  = 0.6f;
engine.processBlock(inputPtrs, outputPtrs, numChannels, numSamples, params);
```
