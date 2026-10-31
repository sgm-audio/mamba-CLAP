# Production readiness assessment — **NOT READY**

**Reviewed:** 2026-10-04
**Status:** research/prototype. This document replaces earlier unchecked “production-ready” assertions. No component in this checkout is currently verified for release.

The previous version of this file marked requirements as met without reproducible build, trained-model, host, audio-quality, or real-time evidence. Those checkmarks were not substantiated and should not be used as release evidence.

## Evidence-based readiness matrix

| Gate | Status | Evidence / gap |
|---|---|---|
| One selected implementation | **In progress** | The repository contains several distinct paths: `MambaDAW`, `clap-plugin`, and `plugin/`. Near-term focus is the custom C++ SSM core in `MambaDAW`; the other paths remain research/legacy experiments until consolidated or retired. |
| Clean build from checkout | **Blocked / unverified** | `MambaDAW/src/plugin_processor.h` includes missing `src/inference_thread.h`. `plugin/CMakeLists.txt` references multiple absent source files. Full CMake/Ninja builds were unavailable in the audit environment. |
| CLAP discovery and lifecycle in a real host | **Not verified** | No host-validation report or reproducible tested binary is recorded. Source files or a tracked binary do not prove the current source builds or passes a host. |
| Mamba-3 integration | **Not implemented in the C++ audio path** | Upstream Python Mamba-3 source exists under `mamba/`; it is not the custom MambaDAW C++ layer. No Mamba-3 checkpoint-to-runtime parity is shown. |
| Trained audio model and train/export/runtime parity | **Not verified** | The default MambaDAW waveform trainer uses `d_model=1`; the processor constructs an 80-dimensional mel-frame model. The old Python trainer bypasses the SSM recurrence. No deterministic Python/C++ golden-vector test is present. |
| LoRA / adapter workflow | **Not implemented** | No repository implementation of Mamba-3 LoRA training, adapter packaging, plug-in loading, or RunPod worker deployment was found. |
| Audio quality | **Not verified** | No controlled paired-data evaluation, blind listening results, or reproducible quality metrics are recorded for a trained model. |
| Hard-real-time behavior | **Not verified; some paths contradict the claim** | `clap-plugin/src/clap-plugin.cpp` performs Torch/STFT/model work in the callback. `plugin/clap/MambaFXPlugin.cpp` allocates during reset. The `plugin/` worker path is incomplete and does not currently wire or consume results end to end. |
| Worst-case latency / dropout behavior | **Not measured** | A complexity statement or an isolated sample-time note is not a deadline test. No measured worst-case callback data across block sizes, sample rates, instances, and target CPUs is available. |
| Allocation/lock audit | **Not run for a complete plug-in** | The standalone layer test is not an allocation audit of a host-loaded plug-in. The experimental INT8 path lazily allocates on its first processing call. |
| Formats, packaging, and signing | **Unverified** | CLAP-oriented code is present, but no release artifact/host validation is established. Optional VST3/LV2 settings and AAX plans are not verified products. |

## Tests actually run

- `MambaDAW/tests/test_mamba_layer.cpp` was compiled directly with GCC and `MambaDAW/src/mamba_layer.cpp`; its seven narrow cases printed as passed.
- That result does not establish plug-in integration, model correctness, sound quality, or deadline safety. The test harness has been corrected to return non-zero on failed cases and to use identical deterministic weights for the block-vs-sample comparison.
- Full CMake/Ninja configuration and build were not run because those tools were absent. Python/model execution was not run because PyTorch was absent.

For the exact standalone command and scope, see [`docs/BUILD_AND_TEST_GUIDE.md`](./BUILD_AND_TEST_GUIDE.md). For the larger audit, see [`docs/CLAIMS_VERIFICATION_2026-10.md`](./CLAIMS_VERIFICATION_2026-10.md).

## Release gate

Do not use “production-ready,” “real-time safe,” “Mamba-3 plug-in,” “60-second musical memory,” “sub-5-ms,” “no dropouts,” or audio-quality claims as established facts until the relevant evidence exists. A release candidate must at minimum:

1. Build from a clean checkout in CI with all required sources and dependencies declared.
2. Load a trained model with a versioned, validated format and pass deterministic Python/C++ parity tests.
3. Pass CLAP validation and lifecycle/audio tests in named DAW hosts.
4. Pass an audio-thread allocation/lock audit and worst-case timing tests on target CPUs, host block sizes, rates, and multiple instances.
5. Demonstrate audio quality with documented baselines, paired or otherwise justified data, objective metrics, and blind listening.
6. Include packaging, model licensing/attribution, supported-host, and distribution documentation.

Current overall assessment: **Research / prototype — not ready for production use.**
