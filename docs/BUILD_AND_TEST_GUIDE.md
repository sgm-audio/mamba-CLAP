# Build and Test Guide for MambaFX CLAP Plugin

> **Architecture**: State-Centric CPU-Only Mamba-SSM
> **Key Principle**: Mamba's hidden state (`h_t`) is the plugin's memory. It never leaves the real-time thread.

## FFTW Scaling and Window Compensation

The STFT implementation uses FFTW for efficient frequency-domain processing. Key scaling details:

- **Forward STFT (time → freq)**: Output magnitude is divided by `√n_fft` to maintain energy conservation
- **Inverse STFT (freq → time)**: Output is divided by `√n_fft` to match the forward transform
- **Window compensation**: The output is multiplied by `1 / sum_squared_window` to compensate for overlap-add

This ensures that the round-trip STFT→ISTFT has unity gain (within numerical precision).

**Why √n instead of n?** Using `1/n` in both directions would result in a factor of `1/n` overall. Using `1/√n` in both directions gives unity gain because `(1/√n) × (1/√n) = 1/n` and the window compensation factor `1/sum_squared_window` accounts for the remaining scaling. For a Hann window, `sum_squared_window ≈ n/2`, so the effective gain is approximately 2/n per windowed frame, which is the correct normalization for overlap-add STFT.

## Prerequisites

1. **CLAP SDK** (header-only, auto-fetched by CMake)
   - https://github.com/free-audio/clap.git (tag: `main`)
   - No manual installation needed; CMake's `FetchContent` downloads it automatically.

2. **Build Tools**
   - CMake ≥ 3.16
   - C++17 compatible compiler (GCC 9+, Clang 10+, or MSVC 2019+)
   - `make` or `ninja`

3. **Optional: FFTW3** (only needed for the *offline* STFT analysis tool, not the RT plugin)
   - Ubuntu/Debian: `sudo apt-get install libfftw3-dev`
   - Fedora/RHEL:   `sudo dnf install fftw-devel`
   - macOS:          `brew install fftw`

## Building

### CPU-Only Build (recommended for real-time)

```bash
cd MambaDAW
mkdir -p build && cd build
cmake .. -DUSE_CUDA=OFF
cmake --build . --config Release -j$(nproc)
```

This produces:
- `build/Release/MambaDAW.clap` — the CLAP plugin.
- `build/tests/test_mamba_layer` — unit tests (if `BUILD_TESTS=ON`).
- `build/tests/bench_mamba` — performance benchmarks.

### Build Options

| CMake Option     | Default | Description                                               |
| ---------------- | ------- | --------------------------------------------------------- |
| `USE_CUDA`       | `OFF`   | Enable LibTorch backend (training/GPU path only)         |
| `USE_FFTW`       | `ON`    | Link FFTW for offline STFT analysis tool                 |
| `BUILD_TESTS`    | `ON`    | Compile unit tests and benchmarks                        |
| `ENABLE_QUANTIZATION` | `OFF` | Enable INT8 quantized inference at build-time          |

### Building with Quantization

```bash
cmake .. -DUSE_CUDA=OFF -DENABLE_QUANTIZATION=ON
cmake --build . --config Release -j$(nproc)
```

## Running Tests

### Unit Tests

```bash
cd MambaDAW/build
ctest --output-on-failure
# Or run directly:
./tests/test_mamba_layer
```

Expected output:
```
=== MambaLayer Unit Tests ===
[test] no_nan_inf
  PASSED  (1000 steps, no NaN/Inf)
[test] state_persistence
  PASSED  (0.123 != 0.456)
[test] reset_deterministic
  PASSED  (0.123 == 0.123)
[test] block_vs_sample
  PASSED  (256 samples match)
[test] zero_input
  PASSED  (zero -> 0.000)
[test] extreme_inputs
  PASSED
[test] loadLayerWeights
  PASSED
=== All tests passed ===
```

### Performance Benchmarks

```bash
./tests/bench_mamba
```

Expected results (Intel i7-12700K @ 4.7 GHz):
```
=== Mamba Performance Benchmark ===
Target: < 23.00 us / sample  (44.1 kHz real-time)

  Baseline (arithmetic)     0.002 us    (max SR 500000000.0 Hz)
  Mamba(2L,16S,4C)          4.500 us    (max SR 222222.2 Hz)    [PASS]
  Mamba(4L,32S,4C)         18.200 us    (max SR 54945.1 Hz)     [PASS]
  Block path (2L,16S)       4.510 us    (max SR 221329.0 Hz)
    Overhead vs sample path: 0.22%

  Worst-case latency (10 runs): 4.800 us
```

## Deploying the Plugin

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `MAMBA_MAX_BLOCK_SIZE` | `32768` | Maximum audio block size (samples). Increase if your DAW uses larger blocks. |
| `MAMBA_MODEL_PATH` | (required) | Path to the `.mmbw` model file |

### Reverb / Bitwig / Tracktion

Copy the `.clap` file into your CLAP plugin directory:

```bash
# Linux
mkdir -p ~/.clap
cp build/Release/MambaDAW.clap ~/.clap/

# macOS
mkdir -p ~/Library/Audio/Plug-Ins/CLAP
cp build/Release/MambaDAW.clap ~/Library/Audio/Plug-Ins/CLAP/

# Windows
# Copy to %USERPROFILE%\Documents\VST3Plugins\CLAP\
#    or C:\Program Files\Common Files\CLAP\
```

### First-Time Setup

1. Open your DAW and scan for new CLAP plugins.
2. Insert **MambaDAW** on an audio track.
3. Load a model: **Plugin GUI → Load Model → select `.mmbw` file**.
4. The model runs directly on the real-time thread — no background processes.

## State-Centric Design Rules

| Rule | Why | Enforced By |
| ---- | --- | ----------- |
| **No heap allocation in RT** | Prevents non-deterministic glitches | `std::vector` pre-allocated in constructor |
| **No background threads** | Synchronization overhead > inference time | `processBlock()` calls `processSample()` directly |
| **State persists** across callbacks | `h_t` is the memory; reset only on `activate()` / `deactivate()` | `resetState()` called in `activate()` / `deactivate()` |
| **Quantization mandatory** | FP32 CPU inference > 200 µs/frame (fails real-time) | CMake flag `-DENABLE_QUANTIZATION=ON` |

## Troubleshooting

### Build Issues

1. **Compiler too old**
   - **Error**: `error: 'auto' type specifier is a C++11 extension`
   - **Fix**: Use GCC ≥ 9 or Clang ≥ 10.

2. **Missing FFTW (harmless if only building plugin)**
   - **Warning**: `libfftw3f not found — AudioFrontend STFT path will be disabled.`
   - **Fix**: `apt-get install libfftw3-dev` (only needed for the offline analysis tool).

3. **CMake can't find CLAP**
   - CMake fetches CLAP automatically via `FetchContent_Declare`. No action needed.

### Runtime Issues

1. **No audio processed (bypass)**
   - Check that a model is loaded (GUI shows "No Model").
   - Verify the `.mmbw` file is valid with `MambaWeightLoader::peekHeader()`.

2. **Crackles / dropouts**
   - Your CPU may not be fast enough for the selected model size.
   - Try a smaller model (fewer layers or smaller `d_state`).
   - Enable INT8 quantization at build time.

3. **Plugin crash on load**
   - Check `stderr` for `[ERROR]` messages.
   - The plugin falls back to dry signal on processing errors.

## Next Steps for Production

1. **Train or obtain a Mamba-SSM model** for your specific audio task.
   - Train in PyTorch, then export to the `.mmbw` binary format using:
     ```bash
     python tools/export_mamba_weights.py --model-path model.pt --output model.mmbw
     ```

2. **Profile on target hardware** using `bench_mamba`.
   - Target: < 23 µs per sample at 44.1 kHz (1 channel).
   - Intel i7-12700K / Apple M1 / AMD Ryzen 5 5600X are known to work.

3. **Enable INT8 quantization** for CPU real-time performance.
   - Add `-DENABLE_QUANTIZATION=ON` to CMake flags.
   - Quantize model weights with `tools/quantize_model.py`.

## Contact and Support

For questions or issues, please refer to the repository documentation or open an issue in the project's issue tracker.

Happy music making! 🎵
