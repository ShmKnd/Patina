# Bindings Status

Updated: 2026-07-08

## C API

The C API exposes the stable engine-level Patina surface through opaque handles:

- BBD delay
- Drive
- Reverb
- Compressor
- Modulation
- Tape machine
- Channel strip
- EQ
- Limiter
- Filter
- Envelope generator

Standalone L3 circuit modules remain C++ API modules for this release. The C API
does not yet expose direct handles for `OtaLadderFilter`, `AcidLadderFilter`,
`AllPassFilter`, or `VocoderBand`.

`VocoderBand` remains an Experiment module and is intentionally not exported
through the C API or Rust safe wrapper in this pass.

## Rust Binding

The Rust crate version is aligned with Patina 1.1.0.

The Rust `ffi` module mirrors the current C API declarations, opaque handles,
parameter structures, functions, and public enum constants. The safe wrapper
exposes owned engine wrappers for every current C API engine and helper methods
for meter/gate/envelope accessors.

## Pure Data External

`bindings/pd` contains the optional `[patina~]` Pure Data external.

The external is built only when `PATINA_BUILD_PD_EXTERNALS=ON` and
`PD_INCLUDE_DIR` points to a Pure Data include/source directory containing
`m_pd.h`. The local development machine used for this pass did not have
`m_pd.h` installed, so the CMake target was added and configured but the Pd
binary was not built from a local Pd installation.

The first Pd surface is mono and engine-oriented:

- `[patina~ drive]`
- `[patina~ filter]`
- `[patina~ delay]`
- `[patina~ reverb]`
- `[patina~ compressor]`
- `[patina~ modulation]`
- `[patina~ tape]`
- `[patina~ eq]`
- `[patina~ limiter]`
- `[patina~ channelstrip]`
- `[patina~ envelope]`

Parameters are sent with `param <name> <value>` messages. Envelope gate control
uses `gate 1` and `gate 0`.

For direct engine-specific option-click help, the public package also includes
alias abstraction files:

- `[patina-drive~]`
- `[patina-filter~]`
- `[patina-delay~]`
- `[patina-reverb~]`
- `[patina-compressor~]`
- `[patina-modulation~]`
- `[patina-tape~]`
- `[patina-eq~]`
- `[patina-limiter~]`
- `[patina-channelstrip~]`
- `[patina-envelope~]`

The Pd/PlugData help lookup is class-name based, so `[patina~ filter]` opens the
common `patina~-help.pd` patch while `[patina-filter~]` opens
`patina-filter~-help.pd`.
