# Repository guidance for Mamba-CLAP

> **Status (2026-10-04):** research/prototype repository; no production-ready plug-in is verified. The code is split across an upstream Mamba reference tree, experimental Python audio-model code, and several distinct/incomplete C++ plug-in paths. See [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md) and [`docs/CLAIMS_VERIFICATION_2026-10.md`](docs/CLAIMS_VERIFICATION_2026-10.md).

## Component boundaries

- `mamba/`: vendored upstream Mamba-family research/reference code. It is not integrated with this repository's C++ plug-in runtime.
- `mamba-compressor/`: experimental Python compressor model/training code; no verified pretrained loader or end-to-end inference example.
- `MambaDAW/`: custom recurrent SSM prototype. It is not upstream selective Mamba/Mamba-3. The standalone layer test is the only verified local C++ test; the full plug-in build is blocked.
- `clap-plugin/`: separate LibTorch/TorchScript experiment; its audio callback performs Torch/STFT work and is not hard-real-time safe.
- `plugin/`: incomplete architecture experiment with missing build sources; not a shipped plug-in.

Do not describe these components as one working train → export → load → real-time audio pipeline.

## Verified narrow test

From the repository root, with a C++17 compiler, run the standalone custom-layer harness:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -IMambaDAW/src \
  MambaDAW/tests/test_mamba_layer.cpp \
  MambaDAW/src/mamba_layer.cpp \
  -o /tmp/test_mamba_layer
/tmp/test_mamba_layer
```

A pass validates only that isolated custom layer. It does not validate a plug-in build, host, pretrained model, Mamba-3, LoRA, audio quality, or real-time behavior. Full build blockers and test scope are in [`docs/BUILD_AND_TEST_GUIDE.md`](docs/BUILD_AND_TEST_GUIDE.md).

## Development guidance

- Check [`AGENTS.md`](AGENTS.md) and the status documents before relying on historical commands or architecture notes.
- Keep research, architecture proposals, and implementation status explicitly distinct.
- Treat performance and business estimates as hypotheses until reproduced; do not promise production readiness, audio quality, latency, memory, revenue, or employment outcomes.
- Add tests and record exact commands/results for code changes; avoid calling source-level intent a safety guarantee.
