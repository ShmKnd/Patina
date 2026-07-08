# Patina 1.1.0 Release Inventory - 2026-07-08

This document records the current Dev/Public alignment point for the Patina library.

## Repository Roles

| Repository | Role | Notes |
|---|---|---|
| `AnalogDSP_kit` / Dev | Private development source | May contain internal implementation notes and local-only workspace history. |
| `ShmKnd/Patina` / Public | Public source | Receives the same usable C++17 library surface with public-facing comments and release notes. |

## Version

- Current library version: `1.1.0`
- Previous public baseline: `1.0.0`
- Reason for minor bump: new filter circuit APIs, API aggregation update, stability/performance improvements, and an explicitly experimental vocoder circuit.

## Newly Inventoried Implementation Since Public Baseline

| Area | Status | Notes |
|---|---|---|
| `OtaLadderFilter` | Public API | 4-pole OTA ladder filter with ZDF/TPT integration, per-stage mismatch, temperature drift, and OTA nonlinearity. |
| `AcidLadderFilter` | Public API | 3-pole transistor ladder variant with input HPF, resonance-dependent BP blend, and ZDF/TPT processing. |
| `AllPassFilter` | Public API | 1st-order TPT all-pass section cascade, up to four stages. |
| `OtaSKFilter` | Stabilized | Reworked from forward-Euler behavior into a TPT SVF-style solver and fixed the high-resonance/high-cutoff instability path. |
| `LadderFilter` / `DiodeLadderFilter` | Stabilized | Replaced forward-Euler ladder integration with ZDF/TPT processing. |
| `FilterEngine` | Documented | Remains an SVF/ToneFilter/Ladder integrated engine; the new filters are documented as standalone L3 circuit modules. |
| Analog noise/drift paths | Performance | Replaced per-sample `std::normal_distribution` usage with cheaper lower-rate Gaussian updates where the analog behavior permits it. |
| macOS metadata | Cleanup | Public tree excludes local `.DS_Store` metadata. |

## Experiment

`VocoderBand` remains an **Experiment**. It is included for evaluation and integration testing, but it should not be described as a stable production vocoder engine yet.

## Documentation Sync

- `README.md`: feature inventory and filter list updated.
- `CHANGELOG.md`: new `1.1.0` entry added with Dev/Public synchronization notes.
- `docs/API_REFERENCE.md`: filter sections now include `OtaLadderFilter`, `AcidLadderFilter`, and `AllPassFilter`; `VocoderBand` is marked experimental.
