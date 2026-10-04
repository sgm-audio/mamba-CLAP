# Build and test guide — current verified scope

**Status (2026-10-04):** A full plug-in build is not verified and is currently blocked by missing source/header dependencies. The old version of this guide listed CMake options, expected benchmark output, deployment steps, and quantization behavior that do not match the checked-in CMake/source tree; do not use those instructions as evidence of a working plug-in.

See [`PROJECT_STATUS.md`](./PROJECT_STATUS.md) for the component inventory and [`CLAIMS_VERIFICATION_2026-10.md`](./CLAIMS_VERIFICATION_2026-10.md) for the complete audit.

## The narrow test that can be run

From the repository root, with a C++17 compiler:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -IMambaDAW/src \
  MambaDAW/tests/test_mamba_layer.cpp \
  MambaDAW/src/mamba_layer.cpp \
  -o /tmp/test_mamba_layer
/tmp/test_mamba_layer
```

This builds only the standalone custom `MambaLayer` test. A pass does **not** validate a CMake build, CLAP wrapper, DAW host, trained weights, Mamba-3, LoRA, audio quality, or real-time deadline/allocation behavior.

## Why the plug-in build is blocked

- `MambaDAW/src/plugin_processor.h` includes `inference_thread.h`, which is absent. The processor also uses `InferenceThread` and `MambaInferenceEngine`; a CMake optional-source check cannot provide a missing header or implementation.
- The default MambaDAW GUI configuration references `../JUCE/modules`, which is not present in this checkout.
- `plugin/CMakeLists.txt` names missing source files, including `model/MambaInferenceEngine.cpp`, `dsp/STFTEngine.cpp`, and `dsp/OverlapAddSynthesizer.cpp`; the default UI/ARA options also reference absent `.cpp` files.
- `clap-plugin/CMakeLists.txt` requires LibTorch. The TorchScript path is a separate experimental implementation and runs Torch operations in its audio callback.

A clean full build should be added to CI after one plug-in path is selected and its missing dependencies are resolved. Until then, do not copy old command snippets that claim a `.clap`, `.vst3`, or `.lv2` artifact is produced.

## Not available as verified commands/results

- No verified `ctest` suite for the full plug-in.
- No verified `bench_mamba` timing, 133 ns/sample result, or latency/dropout pass.
- No verified INT8/INT4 CMake option. The exporter says the C++ loader is FP32-only and currently writes FP32 when INT8 export is requested.
- No verified model-training or Python/C++ parity run in this environment; PyTorch was unavailable.
