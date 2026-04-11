# Installation & Usage Guide

## Quick Start

### 1. Build and Install

```bash
# Clone the repository
git clone <repository-url> Patina
cd Patina

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=~/.local ..

# Build
cmake --build . --config Release -j4

# Install
cmake --install .
```

### 2. Using in a CMake Project

`your_project/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyDSPApp)

find_package(Patina REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE Patina::Patina)
```

`main.cpp`:

```cpp
#include <patina.h>
#include <iostream>

int main()
{
    patina::BbdDelayEngine delay;
    patina::ProcessSpec spec{44100.0, 512, 2};
    delay.prepare(spec);

    std::cout << "Patina library loaded successfully!" << std::endl;
    return 0;
}
```

### 3. Compile and Run

```bash
cd your_project
mkdir build && cd build
cmake ..
make
./myapp
```

---

## Build Options

### Standalone Build (recommended)

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### Linking from a JUCE Plugin

> **Note**: Patina is a JUCE-independent standard library.
> To use it from a JUCE plugin, simply link via `target_link_libraries`.
> No special build flags are needed.

### Unit Tests

```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DPATINA_BUILD_TESTS=ON \
      ..
cmake --build .
ctest --verbose
```

### C Bindings

```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DPATINA_BUILD_C_BINDINGS=ON \
      ..
cmake --build .
# → libpatina_c.a (static) + libpatina_c.dylib/so (shared)
# → patina_c.h installed to include/
```

### Rust Crate

```bash
cd bindings/rust
cargo build --release
cargo test
```

> `build.rs` uses the `cc` crate to automatically compile `patina_c.cpp`,
> so a prior CMake build is not required.

### Running Examples

```bash
cd examples
c++ -std=c++17 -O2 -I.. example_drive_engine.cpp -o example_drive
./example_drive   # → output_drive.wav
```

---

## Platform-Specific Guides

### macOS

```bash
brew install cmake

cmake ..
make
```

### Linux

```bash
sudo apt-get install build-essential cmake

cmake ..
make
sudo make install
```

### Windows (MSVC)

```bash
# Visual Studio 2019/2022 Developer Command Line
cmake -G "Visual Studio 16 2019" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
cmake --install . --config Release
```

---

## Header-Only Usage

If you prefer not to use CMake:

```cpp
#include "../Patina/include/patina.h"

int main()
{
    patina::BbdDelayEngine delay;
    // ...
}
```

---

## Troubleshooting

### "find_package(Patina) not found"

```bash
# Verify installation
ls ~/.local/lib/cmake/Patina/

# Specify CMake prefix path
cmake -DCMAKE_PREFIX_PATH=~/.local ..
```

### Compile Errors

**Verify C++17 support:**

```cmake
target_compile_features(myapp PRIVATE cxx_std_17)
```

**Linking from a JUCE plugin:**

Use `target_link_libraries(my_plugin PRIVATE Patina::Patina)` as usual. No additional flags needed.

---

## Development Build

Debug build with symbols:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug
```

Speed up with ccache:

```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
      ..
```

---

## Uninstall

Remove installed files:

```bash
rm -rf ~/.local/lib/cmake/Patina/
rm -f  ~/.local/include/patina.h
rm -rf ~/.local/include/dsp/
```

Or from the build directory:

```bash
cd build
xargs rm < install_manifest.txt
```

---

## Docker Build Example

```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y build-essential cmake git
RUN git clone <repo> /app && cd /app
RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make && make install
```

```bash
docker build -t patina-lib .
```

---

## FAQ

### Q: Can I just copy the headers and use them directly?

**A:** Yes. Nearly all modules are header-only. However, CMake is recommended for easier dependency management.

### Q: Does it work with compilers other than MSVC on Windows?

**A:** Yes. Tested with MinGW and Clang as well. Any C++17-compliant compiler will work.

### Q: Does it work on Raspberry Pi (ARM)?

**A:** Yes. Use an ARM cross-compiler:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/arm-toolchain.cmake ..
```

### Q: How do I integrate with JUCE?

**A:** Simply include the headers and link the library. No JUCE-specific compatibility mode is needed.

---

## Next Steps

- [API Reference](docs/API_REFERENCE.md)
- [Examples](examples/)
- [CHANGELOG](CHANGELOG.md)
