# MambaDAW — C++ SSM / CLAP prototype

> **Status (2026-10-04): prototype / in progress; not a release plug-in.** The custom C++ layer is not upstream Mamba or Mamba-3. The full CLAP processor is not build-verified: `src/plugin_processor.h` includes the absent `src/inference_thread.h`, and the associated thread/engine implementation is missing. The default GUI configuration also expects a JUCE modules directory that is not present in this checkout. See [`docs/PROJECT_STATUS.md`](../docs/PROJECT_STATUS.md).

## What is present

- `src/mamba_layer.h/.cpp` and `src/mamba_model.h/.cpp`: custom recurrent SSM prototype with preallocated FP32 processing buffers.
- `src/mamba_weight_loader.*`: custom binary weight-loading code.
- `tools/train_audio_mamba.py` and `tools/simple_mamba.py`: separate PyTorch prototype for a synthetic reverb task.
- `python_training/train_mamba.py`: an older synthetic-data experiment. Its simplified layer bypasses the SSM recurrence; do not use it as a trained Mamba implementation.
- `tests/test_mamba_layer.cpp`: narrow standalone layer tests.

The custom layer uses learned, input-independent `dA`, `dB`, and `C` arrays. It does not implement the input-dependent selectivity of upstream Mamba/Mamba-3. The default waveform trainer uses `d_model=1` and 4,096-sample sequences, while the plug-in processor constructs an 80-dimensional mel-frame model. The trainer/export and plug-in model contracts are therefore not yet aligned.

## Test that is currently supported

With GCC and no PyTorch dependency, the layer test can be compiled directly from the repository root:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -IMambaDAW/src \
  MambaDAW/tests/test_mamba_layer.cpp \
  MambaDAW/src/mamba_layer.cpp \
  -o /tmp/test_mamba_layer
/tmp/test_mamba_layer
```

This test only checks the isolated layer. It does **not** demonstrate a complete CMake build, CLAP host behavior, model quality, Mamba-3 integration, LoRA, audio quality, or real-time safety. See [`docs/BUILD_AND_TEST_GUIDE.md`](../docs/BUILD_AND_TEST_GUIDE.md) for verified scope and blockers.

## Not currently supported or verified

- A complete MambaDAW CMake/CLAP build.
- A functioning asynchronous inference engine in this checkout.
- A matching Python-training → custom binary → C++ inference contract.
- INT8/INT4 production quantization. The exporter documents FP32-only C++ loading and currently falls back to FP32 for INT8 export. The INT8 runtime path is experimental and lazily allocates buffers; do not call it on an audio thread as-is.
- Any “60-second musical memory,” low-latency, no-dropout, or production-quality claim.

## Build status

The intended CMake build is **in progress and currently blocked** by the missing inference-thread dependency. Do not use older build instructions that pass `USE_CUDA`, `ENABLE_QUANTIZATION`, or otherwise promise a plug-in artifact; those options/output claims do not match the checked-in CMake configuration. The standalone test above is the only build command verified for this project in the audit environment.
