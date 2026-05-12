# MambaFX CLAP Plugin

## Project
C++17 CLAP audio plugin with LibTorch inference and FFTW3 STFT processing.

## Build Environment
- OS: Bazzite Linux (immutable). Build inside distrobox `build-env` (Ubuntu 22.04).
- LibTorch: /home/SGM/.local/lib/python3.10/site-packages/torch/share/cmake/Torch
- FFTW3: installed in distrobox via apt
- Model: /home/SGM/dev/Audio Development/Mamba Based/models/mamba3_audio_effects.pt

## Build Command
```bash
cd '/home/SGM/dev/Audio Development/Mamba Based/clap-plugin'
rm -rf build && mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DTorch_DIR="/home/SGM/.local/lib/python3.10/site-packages/torch/share/cmake/Torch" \
  -DCMAKE_CXX_FLAGS="-I/usr/include/python3.10 -D_GLIBCXX_USE_CXX11_ABI=1"
cmake --build . -j$(nproc)
```

## Install
```bash
cp build/Release/MambaClapPlugin.clap ~/.clap/MambaFX.clap
```

## Known Issues To Fix
1. `processEffect` in clap-plugin.cpp is missing `MambaWrapper::` class prefix
2. `clap_entry` is declared as a function but CLAP SDK requires it as extern const variable
3. File has duplicate clap_entry definitions - remove the old ClapEntry block entirely

## Key Files
- src/clap-plugin.cpp — main implementation
- include/clap-plugin.hpp — class definitions
- clap-sdk/ — local CLAP headers (do not use FetchContent)

## Rules
- Never run cmake or python scripts from the build/ directory
- Always cd to clap-plugin/ first
- MAMBA_MODEL_PATH must be set before launching REAPER
