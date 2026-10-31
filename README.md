# Mamba Based

**Research and prototypes for state-space models and audio processing**

> **Status (2026-10-31): research/prototype only.** This repository is not a production-ready Mamba-3 audio plug-in. No checked-in plug-in has passed a reproducible full build, host validation, trained-model parity test, audio-quality evaluation, or real-time deadline/allocation audit. See the [project status](./docs/PROJECT_STATUS.md) and [claims audit](./docs/CLAIMS_VERIFICATION_2026-10.md).

This is a multi-project repository. Its model research, Python audio experiments, custom C++ SSM, and CLAP experiments are separate code paths; they are not currently one integrated train → export → load → real-time audio pipeline.

## Component status

| Path | Status | What is present |
|---|---|---|
| [`mamba/`](./mamba) | **Research/reference** | Vendored upstream Mamba-family Python implementation, including Mamba-3 source. Not wired to the C++ plug-in paths. |
| [sgm-audio/mamba-compressor](https://github.com/sgm-audio/mamba-compressor) | **Moved** | The `mamba-compressor/` folder here was an older copy of that repo. It was removed on 2026-10-31; use the standalone repo. |
| [`MambaDAW/`](./MambaDAW) | **C++ prototype / in progress** | Custom recurrent SSM and CLAP-related code. The custom layer is not upstream Mamba-3; the plug-in path is blocked by a missing inference-thread header/source. |
| [`clap-plugin/`](./clap-plugin) | **Legacy research experiment** | TorchScript/LibTorch CLAP experiment. Torch operations run in its audio callback; do not treat it as real-time safe. |
| [`plugin/`](./plugin) | **Architecture prototype / in progress** | A separate CLAP/worker design with missing CMake-listed sources and incomplete model/result wiring. |
| [`docs/`](./docs) | **Status and design notes** | See the status page before relying on older architecture, implementation, or readiness documents. |

## What is and is not demonstrated

- The original Mamba research supports linear sequence-length scaling for the relevant SSM algorithms. That asymptotic property alone does **not** prove a real-time callback deadline, useful 60-second musical memory, or audio quality.
- `MambaDAW/src/mamba_layer.*` is a custom fixed-coefficient recurrent SSM prototype, not the upstream selective Mamba or Mamba-3 implementation.
- One standalone C++ layer test is available. It does not validate a plug-in build, a DAW host, Mamba-3, LoRA, trained model behavior, or hard-real-time performance.
- LoRA fine-tuning/export, RunPod deployment, and a commercial adapter product are **not implemented** in this repository.

## Start here

1. Read [`docs/PROJECT_STATUS.md`](./docs/PROJECT_STATUS.md) for component status and release gates.
2. Read [`docs/CLAIMS_VERIFICATION_2026-10.md`](./docs/CLAIMS_VERIFICATION_2026-10.md) for claim-by-claim findings and sources.
3. For the narrow standalone layer test, see [`docs/BUILD_AND_TEST_GUIDE.md`](./docs/BUILD_AND_TEST_GUIDE.md). A full plug-in build is currently blocked and has not been verified.

## What builds and passes (2026-10-31)

Shared 8-vCPU Xeon VM, Linux, zig clang 22.1.8, rustc 1.99.1. Run `bash scripts/ci.sh` (set `CXX` to pick the compiler).

| Check | Before cleanup (`f5a0158`) | After |
|---|---|---|
| `cargo test` (root crate `mamba_prompt_system`) | 22 passed, 4 failed | 28 passed, 0 failed |
| `MambaDAW/tests/test_mamba_layer.cpp`, `-O2 -Wall -Wextra -Wpedantic -Werror` | did not compile (`-Wimplicit-const-int-float-conversion`); 7/7 without `-Werror` | 7/7 |
| `MambaDAW/tests/param_gate_test.cpp`, `-O2 -Werror` | not run before | passes |
| MambaDAW plug-in, `plugin/`, `clap-plugin/` | not built | not built; blockers in [`docs/PROJECT_STATUS.md`](./docs/PROJECT_STATUS.md) |

The 4 Rust fixes: `RingBuffer::new(n)` now holds n items (it held n-1), `CrossfadeBuffer::reset()` restores the zeroed buffer, and the crossfade test checks the ramp the code implements (its old expected values could not come from any linear fade). There is no GitHub Actions workflow.

## Where the maintained code is

| Topic | Repo |
|---|---|
| Real-time SSM CLAP effect (C++) | [sgm-audio/EmT](https://github.com/sgm-audio/EmT) |
| CLAP/VST3 SSM processor, ONNX/C++ engine | [sgm-audio/clara](https://github.com/sgm-audio/clara) |
| Rust Mamba-3 experiments | [sgm-audio/mamba3](https://github.com/sgm-audio/mamba3) |
| Python Mamba compressor experiments | [sgm-audio/mamba-compressor](https://github.com/sgm-audio/mamba-compressor) |

## Layout

- `mamba/`: vendored state-spaces/mamba (Apache-2.0, its own LICENSE).
- `MambaDAW/`: C++ prototype; the standalone tests above live in `MambaDAW/tests/`.
- `clap-plugin/`: legacy TorchScript CLAP experiment. `clap-plugin/clap-sdk` is a submodule of free-audio/clap at tag 1.2.7 (`git submodule update --init`).
- `plugin/`, `mastering_service/`, `backend/`, `ara-intelligence-*`: design prototypes, not built here.
- `src/`: the Rust crate. `scripts/`: `ci.sh`, `run-tests.sh`.
- `docs/`: status and design notes; `docs/design/` (PRD, system design, test plan), `docs/api/`, `docs/history/` (superseded summaries, kept for reference).

## Research goals

Explore whether state-space models can help predict audio-effect parameters from musical context, and determine whether a small, properly trained model can run within a measured host callback budget. These are research questions, not current product claims.

## License

This project is licensed under the MIT License; check individual subdirectories for their own license files and third-party notices.
