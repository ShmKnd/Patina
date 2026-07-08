# Patina Pure Data External

`patina~` is a mono Pure Data external backed by the Patina C API.

## Build

Pass a Pure Data source/include directory that contains `m_pd.h`.

```sh
cmake -S . -B build/pd \
  -DPATINA_BUILD_PD_EXTERNALS=ON \
  -DPD_INCLUDE_DIR=/path/to/pure-data/src
cmake --build build/pd --target patina_tilde
```

The output file is named for the host platform:

- macOS: `patina~.pd_darwin`
- Linux: `patina~.pd_linux`
- Windows: `patina~.dll`

## Usage

Create the generic object with an engine argument, or use an engine alias:

```pd
[patina~ drive]
[patina~ filter]
[patina~ delay]
[patina-drive~]
[patina-filter~]
[patina-delay~]
```

Supported engines are `drive`, `filter`, `delay`, `reverb`, `compressor`,
`modulation`, `tape`, `eq`, `limiter`, `channelstrip`, and `envelope`.

Pd and PlugData help lookup is class-name based and does not inspect object
arguments. Option-clicking `[patina~ filter]` therefore opens the common
`patina~-help.pd` patch. Use the alias objects such as `[patina-filter~]` or
`[patina-delay~]` when you want option-click to open an engine-specific help
patch directly.

The alias objects are provided as Pd abstractions that wrap `[patina~ <engine>]`.
Keep `patina~.pd_darwin` / `patina~.pd_linux` / `patina~.dll`, the
`patina-<engine>~.pd` abstraction files, and the `*-help.pd` files together in
the same Pd search path.

Set parameters with `param` messages:

```pd
[param drive 0.7(
[param tone 0.45(
[param mix 1(
```

For `[patina~ envelope]`, use `gate 1` and `gate 0` to trigger the envelope.

The first bare float is mapped to the most common control for a few engines:
`drive` for `[patina~ drive]`, `cutoff` for `[patina~ filter]`, and `delay_ms`
for `[patina~ delay]`.

### Filter Engine Notes

`[patina~ filter]` is not limited to LPF. Use numeric filter and slope values:

- `param type 0`: filter 1 LPF
- `param type 1`: filter 1 HPF
- `param type 2`: filter 1 BPF
- `param type 3`: filter 1 ladder
- `param slope 0`: 6 dB/oct
- `param slope 1`: 12 dB/oct
- `param slope 2`: 18 dB/oct
- `param slope 3`: 24 dB/oct

The second filter uses `type2`, `slope2`, `cutoff2`, and `resonance2`.
Routing is `param routing 0` for serial and `param routing 1` for parallel.

## Help Patches

The Pd documentation patches are:

- `patina~-help.pd`
- `patina~-drive-help.pd`
- `patina~-filter-help.pd`
- `patina~-delay-help.pd`
- `patina~-reverb-help.pd`
- `patina~-compressor-help.pd`
- `patina~-modulation-help.pd`
- `patina~-tape-help.pd`
- `patina~-eq-help.pd`
- `patina~-limiter-help.pd`
- `patina~-channelstrip-help.pd`
- `patina~-envelope-help.pd`
- `patina-drive~-help.pd`
- `patina-filter~-help.pd`
- `patina-delay~-help.pd`
- `patina-reverb~-help.pd`
- `patina-compressor~-help.pd`
- `patina-modulation~-help.pd`
- `patina-tape~-help.pd`
- `patina-eq~-help.pd`
- `patina-limiter~-help.pd`
- `patina-channelstrip~-help.pd`
- `patina-envelope~-help.pd`

The `patina-<engine>~` help files are for the alias abstractions. They are the
recommended route when engine-specific option-click help is more important than
the compact `[patina~ <engine>]` form.

The matching abstraction files are:

- `patina-drive~.pd`
- `patina-filter~.pd`
- `patina-delay~.pd`
- `patina-reverb~.pd`
- `patina-compressor~.pd`
- `patina-modulation~.pd`
- `patina-tape~.pd`
- `patina-eq~.pd`
- `patina-limiter~.pd`
- `patina-channelstrip~.pd`
- `patina-envelope~.pd`
