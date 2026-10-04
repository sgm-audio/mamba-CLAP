# Historical implementation summary — **superseded, not a release record**

The earlier version of this file described the CLAP experiment as complete and production-ready and listed test/model files that are not present in the current checkout. Those claims were not verified and are withdrawn.

## Current status

- `clap-plugin/` is an experimental TorchScript/LibTorch CLAP path. It performs Torch/STFT/model operations from its audio processing callback, and its effect decoders are initialized with random weights. It is not established as a real-time-safe, trained Mamba-3 effect.
- `MambaDAW/` contains a separate custom C++ recurrent SSM prototype. Its full CLAP path is blocked by the missing `src/inference_thread.h` and associated implementation. Its default training dimensions/representation do not match the processor's default model dimensions.
- `plugin/` is another incomplete architecture track; its CMake file lists missing sources.
- `test_stft.cpp`, `test_plugin_integration.cpp`, and `convert_model.py` referenced by the previous version are not present under `clap-plugin/` in this checkout.

No complete plug-in build, DAW host validation, trained-model parity, audio-quality study, or real-time deadline audit is recorded. Do not use this file as evidence that a plug-in is complete or ready to distribute.

See [`docs/PROJECT_STATUS.md`](./docs/PROJECT_STATUS.md), [`PRODUCTION_READINESS.md`](./PRODUCTION_READINESS.md), and [`docs/CLAIMS_VERIFICATION_2026-10.md`](./docs/CLAIMS_VERIFICATION_2026-10.md) for the current status and evidence.
