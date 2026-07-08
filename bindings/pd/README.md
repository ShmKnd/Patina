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

Create an object with the engine name:

```pd
[patina~ drive]
[patina~ filter]
[patina~ delay]
```

Supported engines are `drive`, `filter`, `delay`, `reverb`, `compressor`,
`modulation`, `tape`, `eq`, `limiter`, `channelstrip`, and `envelope`.

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
