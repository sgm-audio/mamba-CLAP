# Mamba-SSM CLAP Plugin: Next Steps & Refactoring Roadmap

## Executive Summary

The previous architecture was fundamentally flawed for real-time audio. It processed entire STFT blocks inside the audio callback using `torch::Tensor`, causing non-deterministic heap allocations and breaking real-time constraints. The new architecture mandates a **State-Centric Design** where the Mamba model runs sample-by-sample inside the real-time thread using only pre-allocated raw arrays.

**Key Mandate:**
- **DO NOT** use a background thread for inference.
- **DO** run the quantized Mamba model **directly on the Real-Time Thread**.
- **DO NOT** allocate memory inside the audio callback.
- **DO** persist state (h_t) across `processBlock()` calls.

---

## Phase 1: Core Kernel Implementation (CRITICAL - Must Complete First)

### 1.1 Fix `plugin_processor.cpp` (Immediate Blocker)

The current `plugin_processor.cpp` (190 lines) is the old, heavy implementation. It must be completely rewritten to match the new `plugin_processor.h` interface.

**Requirements for new `plugin_processor.cpp`:**

```cpp
#include "plugin_processor.h"
#include <algorithm>
#include <cmath>

PluginProcessor::PluginProcessor(int sample_rate) 
    : sample_rate_(sample_rate), bypass_(false), activated_(false), dry_wet_mix_(0.5f), model_type_(0) {
    // Initialize model with small parameters for real-time safety
    // d_model=1, n_layers=2, d_state=16, d_conv=4, expand=2
    mamba_model_ = std::make_unique<MambaModel>(1, 2, 16, 4, 2); 
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::activate(double sample_rate, uint32_t min_frame_count, uint32_t max_frame_count) {
    (void)min_frame_count; (void)max_frame_count;
    sample_rate_ = static_cast<int>(sample_rate);
    activated_ = true;
    if (mamba_model_) {
        mamba_model_->resetState();
    }
}

void PluginProcessor::deactivate() {
    activated_ = false;
    if (mamba_model_) {
        mamba_model_->resetState();
    }
}

void PluginProcessor::processBlock(float** inputs, float** outputs, int32_t num_frames, 
                                 int32_t num_input_channels, int32_t num_output_channels) {
    // Bypass check: copy input to output directly
    if (!activated_ || bypass_ || !mamba_model_) {
        for (int32_t c = 0; c < std::min(num_input_channels, num_output_channels); ++c) {
            std::copy(inputs[c], inputs[c] + num_frames, outputs[c]);
        }
        for (int32_t c = num_input_channels; c < num_output_channels; ++c) {
            std::fill(outputs[c], outputs[c] + num_frames, 0.0f);
        }
        return;
    }
    
    // Process each sample through the Mamba model
    // This is the core real-time loop - must be tight and allocation-free
    for (int32_t i = 0; i < num_frames; ++i) {
        float input_sample = (num_input_channels > 0) ? inputs[0][i] : 0.0f;
        
        // Run through Mamba model (sample-by-sample recurrence)
        float output_sample = mamba_model_->processSample(input_sample);
        
        // Apply dry/wet mixing
        float wet = dry_wet_mix_;
        float dry = 1.0f - wet;
        float mixed = dry * input_sample + wet * output_sample;
        
        // Sanitize output
        if (std::isnan(mixed) || std::isinf(mixed)) {
            mixed = 0.0f;
        }
        
        // Soft clipping
        mixed = std::tanh(mixed);
        
        if (num_output_channels > 0) {
            outputs[0][i] = mixed;
        }
    }
    
    // Copy first channel to all other output channels
    for (int32_t c = 1; c < num_output_channels; ++c) {
        std::copy(outputs[0], outputs[0] + num_frames, outputs[c]);
    }
}

void PluginProcessor::setParamValue(uint32_t param_id, double value) {
    switch (param_id) {
        case 0: dry_wet_mix_ = static_cast<float>(value); break;
        case 1: model_type_ = static_cast<int>(value); break;
        case 2: bypass_ = static_cast<bool>(value); break;
        default: break;
    }
}

double PluginProcessor::getParamValue(uint32_t param_id) const {
    switch (param_id) {
        case 0: return static_cast<double>(dry_wet_mix_);
        case 1: return static_cast<double>(model_type_);
        case 2: return static_cast<double>(bypass_);
        default: return 0.0;
    }
}

bool PluginProcessor::isBypassed() const { return bypass_; }
void PluginProcessor::setBypass(bool bypass) { bypass_ = bypass; }

bool PluginProcessor::loadModel(const std::string& path) {
    (void)path;
    // TODO: Implement loading from custom binary format
    return (mamba_model_ != nullptr);
}
```

### 1.2 Verify `mamba_layer.h` and `mamba_layer.cpp`

- **Already corrected by previous agent.**
- Verify no `torch` includes remain.
- Verify `processSample()` is real-time safe (no heap allocation, no exceptions, no system calls).
- Ensure `reset_state()` uses `std::fill` (atomic, no allocation).

### 1.3 Verify `mamba_model.h` and `mamba_model.cpp`

- **Already corrected by previous agent.**
- Verify `processBlock()` delegates to `processSample()` in a tight loop.
- Ensure `resetState()` propagates to all layers.

---

## Phase 2: Weight Loading System

### 2.1 Create Custom Binary Weight Format

The old system used `torch::jit::load()` which is incompatible with raw arrays. A new format is required.

**Format Specification:**

```
Header (28 bytes):
  - Magic: "MMBW" (4 bytes)
  - Version: uint32_t (4 bytes)
  - Num Layers: uint32_t (4 bytes)
  - D Model: uint32_t (4 bytes)
  - D State: uint32_t (4 bytes)
  - D Conv: uint32_t (4 bytes)
  - Expand: uint32_t (4 bytes)
  - Flags: uint32_t (4 bytes) // Quantization mode, etc.

Per-Layer Weights (sequential):
  - In Proj: [d_model, 2 * d_inner] floats
  - Out Proj: [d_model, d_inner] floats
  - Conv1d: [d_inner, d_conv] floats
  - dA: [d_inner, d_state] floats
  - dB: [d_inner, d_state] floats
  - C: [d_inner, d_state] floats
  - D: [d_inner] floats
```

### 2.2 Implement `MambaWeightLoader`

Create `MambaWeightLoader.cpp` / `.h`:

```cpp
class MambaWeightLoader {
public:
    static bool loadWeights(const std::string& path, MambaModel& model);
    static bool saveWeights(const std::string& path, const MambaModel& model);
    
private:
    static constexpr uint32_t MAGIC = 0x4D4D4257; // "MMBW"
    static constexpr uint32_t VERSION = 1;
};
```

### 2.3 Python Export Utility

Create a Python script (e.g., `tools/export_mamba_weights.py`) that:
1. Loads a trained PyTorch Mamba model.
2. Extracts `dA`, `dB`, `C`, `D`, `W_in`, `W_out`, `W_conv`.
3. Writes them to the custom binary format.
4. Validates round-trip (load back and compare).

---

## Phase 3: Quantization Implementation (CRITICAL for Real-Time Performance)

### 3.1 INT8 Quantization Stubs

Add to `mamba_layer.h`:

```cpp
enum class QuantMode {
    FP32,
    INT8,
    INT4
};

class MambaLayer {
public:
    void setQuantizationMode(QuantMode mode);
    QuantMode getQuantizationMode() const { return quant_mode_; }
    
private:
    QuantMode quant_mode_ = QuantMode::FP32;
    
    // INT8 weights (only populated if quantized)
    std::vector<int8_t> weight_in_proj_q_;
    std::vector<int8_t> weight_out_proj_q_;
    std::vector<int8_t> weight_conv1d_q_;
    std::vector<int8_t> dA_q_;
    std::vector<int8_t> dB_q_;
    std::vector<int8_t> C_q_;
    
    // Per-channel scale factors
    std::vector<float> scale_in_proj_;
    std::vector<float> scale_out_proj_;
    // ... etc
};
```

### 3.2 Quantized `processSample`

Implement `MambaLayer::processSampleQuantized()`:
- Use INT8 weights with per-channel scale factors.
- Dequantize on-the-fly inside the hot loops.
- Accumulate in FP32 to prevent overflow.
- If INT4 is selected, map to INT8 for simplicity initially (INT4 is future work).

### 3.3 Quantization Calibration Script

Create `tools/quantize_model.py`:
- Takes a trained FP32 Mamba model.
- Estimates per-channel scale factors using min/max or percentile methods.
- Validates INT8 accuracy against FP32 on a test set.
- Exports the quantized weights to the custom binary format.

---

## Phase 4: Build System Updates

### 4.1 `CMakeLists.txt` Changes

```cmake
cmake_minimum_required(VERSION 3.18)
project(MambaDAW)

# Options
option(USE_CUDA "Enable CUDA support (training only)" OFF)
option(USE_FFTW "Enable FFTW for audio frontend" ON)
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_QUANTIZATION "Enable INT8/INT4 quantization" OFF)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler optimizations
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -ffast-math")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Disable RTTI/exceptions for smaller binary (optional)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

# Find packages
if(USE_CUDA)
    find_package(CUDA REQUIRED)
    add_definitions(-DUSE_CUDA)
    find_package(Torch REQUIRED)
else()
    message(STATUS "Building CPU-only version")
    add_definitions(-DCPU_ONLY)
endif()

if(USE_FFTW)
    find_package(FFTW3 REQUIRED)
    add_definitions(-DUSE_FFTW)
else()
    message(WARNING "FFTW not found. Audio frontend may not work.")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Source files (exclude CUDA files if CPU_ONLY)
set(CORE_SOURCES
    src/mamba_layer.cpp
    src/mamba_model.cpp
    src/plugin_processor.cpp
    # ... other core files
)

# Create library
add_library(mamba_plugin SHARED ${CORE_SOURCES})

# Link libraries
if(USE_CUDA AND Torch_FOUND)
    target_link_libraries(mamba_plugin ${TORCH_LIBRARIES})
endif()

if(USE_FFTW AND FFTW3_FOUND)
    target_link_libraries(mamba_plugin FFTW3::fftw3)
endif()

# Tests
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

### 4.2 Remove Torch Dependencies from Core

Update all `#include <torch/torch.h>` in core files to be conditional:

```cpp
#ifdef USE_CUDA
#include <torch/torch.h>
#endif
```

Core files must NOT depend on `torch` if `CPU_ONLY` is defined. The `MambaLayer`, `MambaModel`, and `PluginProcessor` must compile with zero torch includes.

---

## Phase 5: Testing & Validation

### 5.1 Unit Tests: `tests/test_mamba_layer.cpp`

```cpp
#include "mamba_layer.h"
#include <cassert>
#include <cmath>
#include <iostream>

void test_sample_processing() {
    std::cout << "test_sample_processing..." << std::endl;
    MambaLayer layer(1, 16, 4, 2);
    
    float out1 = layer.processSample(1.0f);
    float out2 = layer.processSample(0.5f);
    float out3 = layer.processSample(-0.5f);
    
    assert(!std::isnan(out1) && !std::isinf(out1));
    assert(!std::isnan(out2) && !std::isinf(out2));
    assert(!std::isnan(out3) && !std::isinf(out3));
    
    float out4 = layer.processSample(1.0f);
    assert(out4 != out1); // State changed
    std::cout << "  PASSED" << std::endl;
}

void test_reset_state() {
    std::cout << "test_reset_state..." << std::endl;
    MambaLayer layer(1, 16, 4, 2);
    float out1 = layer.processSample(1.0f);
    layer.reset_state();
    float out2 = layer.processSample(1.0f);
    // After reset, should be deterministic (same input -> same output if state is zeroed)
    assert(out1 == out2); 
    std::cout << "  PASSED" << std::endl;
}

void test_block_processing() {
    std::cout << "test_block_processing..." << std::endl;
    MambaLayer layer(1, 16, 4, 2);
    float input[100];
    float output[100];
    for (int i = 0; i < 100; ++i) input[i] = 0.5f * std::sin(0.1f * i);
    
    layer.processBlock(input, output, 100);
    
    for (int i = 0; i < 100; ++i) {
        assert(!std::isnan(output[i]) && !std::isinf(output[i]));
    }
    std::cout << "  PASSED" << std::endl;
}

int main() {
    test_sample_processing();
    test_reset_state();
    test_block_processing();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
```

### 5.2 Performance Benchmarks: `tests/bench_mamba.cpp`

```cpp
#include <chrono>
#include <iostream>
#include "mamba_model.h"

void benchmark_processSample() {
    MambaModel model(1, 4, 16, 4, 2);
    
    const int num_samples = 44100; // 1 second at 44.1kHz
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_samples; ++i) {
        model.processSample(0.5f);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double us_per_sample = static_cast<double>(duration.count()) / num_samples;
    double max_sr = 1.0e6 / us_per_sample;
    
    std::cout << "=== Performance Benchmark ===" << std::endl;
    std::cout << "Sample rate: 44.1 kHz" << std::endl;
    std::cout << "Time per sample: " << us_per_sample << " us" << std::endl;
    std::cout << "Max sample rate: " << max_sr << " Hz" << std::endl;
    std::cout << "Target (<23 us): " << (us_per_sample < 23.0 ? "PASS" : "FAIL") << std::endl;
}

int main() {
    benchmark_processSample();
    return 0;
}
```

### 5.3 Integration Tests

- `tests/test_plugin_processor.cpp`: Test bypass, dry/wet, NaN/Inf handling.
- `tests/test_weight_loader.cpp`: Test save/load round-trip.
- `tests/test_quantization.cpp`: Test INT8 vs FP32 accuracy.

---

## Phase 6: Documentation Updates

### 6.1 Update `AGENTS.md`

Add new section under "Action Items & Next Steps":

```markdown
### Mamba Real-Time Refactoring (In Progress)
- [x] **MambaLayer refactored** – raw arrays, no torch, real-time safe
- [x] **MambaModel refactored** – manages layers, sample-by-sample processing
- [x] **PluginProcessor header refactored** – no torch, no AudioFrontend, no ModelLoader
- [ ] **PluginProcessor implementation** – must be written (see NEXT_STEPS.md)
- [ ] **Weight loader** – custom binary format, no torch dependency
- [ ] **Quantization** – INT8/INT4 for CPU real-time
- [ ] **Build system** – CPU-only CMake target
- [ ] **Unit tests** – verify correctness and real-time safety
- [ ] **Performance benchmarks** – ensure <200us per frame
```

### 6.2 Update `BUILD_AND_TEST_GUIDE.md`

Document:
- CPU-only build: `cmake .. -DUSE_CUDA=OFF`
- FFTW install: `apt-get install libfftw3-dev`
- Expected CPU targets (e.g., Intel i7-12700K, Apple M1)
- Real-time performance targets (<23us per sample at 44.1kHz for 1-channel)
- How to run tests: `cd build && ctest`

### 6.3 Update `README.md`

Add section "State-Centric Architecture":
- Mamba state (h_t) persists in the real-time thread
- No background threads for inference
- Quantization is mandatory for CPU real-time
- Model weights loaded at initialization, never during audio callback

---

## Phase 7: Operational Readiness

### 7.1 Monitoring (Future)

Add metrics (Prometheus counters) for:
- CPU inference time per callback
- Buffer underruns
- NaN/Inf detections
- Model bypass events

### 7.2 CI Pipeline

- Build CPU-only target on Ubuntu, macOS, Windows (via cross-compilation or GitHub Actions runners).
- Run unit tests and benchmarks.
- Valgrind for memory leak detection (optional, very slow).
- Static analysis with `clang-tidy`.

### 7.3 Docker

Create `Dockerfile.cpu`:
```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y build-essential cmake libfftw3-dev
COPY . /app
WORKDIR /app/build
RUN cmake .. -DUSE_CUDA=OFF && make -j$(nproc)
CMD ["./tests/test_mamba_layer"]
```

---

## Detailed Action Items for Next Coding Agent

### IMMEDIATE (Do First)

1. **Write `plugin_processor.cpp`** (CRITICAL BLOCKER)
   - File: `/MambaDAW/src/plugin_processor.cpp`
   - Replace with the real-time-safe implementation shown in Phase 1.1 above.
   - Ensure `mamba_model_->processSample()` is called for every sample.
   - Remove all `torch::Tensor`, `AudioFrontend`, and `ModelLoader` references.
   - Keep only dry/wet mixing and soft clipping in the callback.
   - **DO NOT use `std::cout` inside `processBlock()`**. Use a ring-buffer logger or remove entirely.

2. **Verify `mamba_layer.h` and `mamba_layer.cpp`**
   - Already corrected by previous agent, but double-check.
   - Ensure `processSample()` is real-time safe.
   - Ensure `reset_state()` uses `std::fill` (atomic, no allocation).
   - Verify no `torch` includes remain.

3. **Verify `mamba_model.h` and `mamba_model.cpp`**
   - Already corrected by previous agent, but double-check.
   - Ensure `processBlock()` delegates to `processSample()` in a tight loop.
   - Ensure `resetState()` propagates to all layers.

### SHORT TERM (Next 1-2 Sessions)

4. **Create `MambaWeightLoader` class**
   - Files: `MambaDAW/src/weight_loader.h`, `MambaDAW/src/weight_loader.cpp`
   - Implement binary format reader/writer.
   - Integrate with `MambaModel::loadWeights()`.
   - Create Python script `tools/export_mamba_weights.py` to export trained PyTorch models.

5. **Update `CMakeLists.txt`**
   - File: `MambaDAW/CMakeLists.txt`
   - Make CUDA optional.
   - Add CPU optimization flags.
   - Create CPU-only build target.
   - Add tests subdirectory.

6. **Write unit tests**
   - File: `MambaDAW/tests/test_mamba_layer.cpp`
   - Test `processSample()`, `processBlock()`, and `resetState()`.
   - Verify no NaN/Inf for various inputs.

### MEDIUM TERM (Next 3-5 Sessions)

7. **Implement Quantization Stubs**
   - Add `QuantMode` enum to `mamba_layer.h`.
   - Add INT8 weight storage fields.
   - Implement `processSampleQuantized()` that falls back to FP32 for now.

8. **Performance Benchmarks**
   - File: `MambaDAW/tests/bench_mamba.cpp`
   - Measure time per sample, max sample rate.
   - Compare with target (<23us per sample at 44.1kHz).

9. **Kernel Fusion & SIMD**
   - Fuse the state update and output projection loops.
   - Use AVX2/NEON intrinsics for the inner loops.
   - Profile cache usage with `perf` or VTune.

10. **Weight Loader Integration**
    - Integrate `MambaWeightLoader` with `PluginProcessor::loadModel()`.
    - Add error handling for corrupt/invalid weight files.

### LONG TERM (Future Milestones)

11. **GPU Support (Optional)**
    - Re-enable CUDA path for high-end setups.
    - Implement GPU kernel for `selective_scan`.
    - Add runtime GPU detection.

12. **Advanced Features**
    - Multi-channel processing (stereo, surround).
    - Variable block size support.
    - Preset management.

13. **Production Hardening**
    - Add monitoring metrics (CPU usage, latency).
    - Implement graceful degradation (bypass on error).
    - Add crash recovery.

---

## Priority Matrix

| Task | Priority | Effort | Blocker For |
|------|----------|--------|-------------|
| Write `plugin_processor.cpp` | **CRITICAL** | Medium | Everything |
| Verify `mamba_layer.cpp` | **CRITICAL** | Low | Core correctness |
| Verify `mamba_model.cpp` | **CRITICAL** | Low | Core correctness |
| Update `CMakeLists.txt` | **HIGH** | Low | Build system |
| Weight Loader | **HIGH** | Medium | Model loading |
| Unit Tests | **HIGH** | Medium | Validation |
| Quantization Stubs | **HIGH** | Low | Performance path |
| Performance Benchmarks | **MEDIUM** | Low | Optimization |
| Actual INT8 Implementation | **MEDIUM** | High | Production release |
| SIMD Optimization | **LOW** | High | Final speed |
| GPU Support | **LOW** | Very High | Future feature |

---

## Important Notes for Next Agent

- **Do NOT use `torch` in the real-time path.** Only use it for weight export/training tools.
- **Do NOT allocate memory in `processBlock()` or `processSample()`**.
- **The Mamba model runs in the audio callback.** This is the design. Optimize the CPU path.
- **If a task seems too large, break it down.** Create stubs and TODOs.
- **Always verify changes compile** before moving to the next task.
- **Test with `tests/test_mamba_layer.cpp`** after every kernel change.
- **Do NOT use `std::cout` in the audio callback.** It's not real-time safe on all platforms.
- **State (h_t) must persist across `processBlock()` calls.** Never reset inside the callback.
- **If the model fails (NaN/Inf), bypass it and return the input directly.** Do not crash.

Good luck. The fate of the real-time thread depends on you.
