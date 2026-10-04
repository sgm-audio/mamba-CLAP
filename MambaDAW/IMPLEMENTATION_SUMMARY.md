# MambaDAW implementation summary — prototype / in progress

This file replaces an older summary that described a different LibTorch architecture and listed source files that do not match the current tree.

## Current implementation

- `src/mamba_layer.*` and `src/mamba_model.*`: custom fixed-coefficient recurrent SSM prototype; not upstream Mamba or Mamba-3.
- `src/mamba_weight_loader.*`: custom binary FP32 weight loader.
- `tools/train_audio_mamba.py`: synthetic reverb training experiment, defaulting to `d_model=1` and 4,096-sample sequences.
- `python_training/train_mamba.py`: older experiment whose simplified layer bypasses the SSM recurrence.
- `tests/test_mamba_layer.cpp`: isolated unit/smoke tests for the custom C++ layer.

## Current blockers

`src/plugin_processor.h` includes `src/inference_thread.h`, which is absent. The corresponding inference-thread/engine implementation is not present. The processor constructs an 80-dimensional mel-frame model, which does not match the default scalar waveform training/export configuration. The default GUI configuration also expects a JUCE modules directory not checked out here.

The only verified build is the standalone layer test. No full plug-in build, host test, Python/C++ parity, audio-quality study, or real-time deadline audit has been completed. INT8 export currently falls back to FP32; the C++ INT8 path is experimental and lazily allocates, so it is not suitable for first-use audio-thread processing.

See [`../docs/PROJECT_STATUS.md`](../docs/PROJECT_STATUS.md) and [`../docs/BUILD_AND_TEST_GUIDE.md`](../docs/BUILD_AND_TEST_GUIDE.md) before using older instructions.
