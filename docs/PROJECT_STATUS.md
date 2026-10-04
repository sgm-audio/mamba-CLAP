# Project status

**Last reviewed:** 2026-10-04  
**Overall status:** **Research / prototype — not release-ready**  
**Release claim:** None. No plug-in has passed a reproducible full build, host validation, trained-model parity, audio-quality evaluation, or real-time deadline/allocation audit in this checkout.

This page is the status source of truth for repository components. Architecture documents, PRDs, summaries, marketing drafts, and code comments may describe goals; they do not override the statuses below.

## Status labels

- **Research:** exploratory model/code; not integrated into a distributable plug-in.
- **Prototype / in progress:** some code exists, but integration, build, tests, or performance evidence is incomplete.
- **Unverified:** a claim or behavior has not been tested with the relevant binary, host, hardware, dataset, or trained model.
- **Production-ready:** reserved for a version with reproducible builds, validated host operation, quality evidence, and measured real-time safety. No component currently has this status.

## Component inventory

| Component | Status | Evidence and current limit | Next proof gate |
|---|---|---|---|
| `mamba/` | **Research/reference code** | Vendored upstream Mamba-1/2/3 Python implementation. It is not linked to a C++ plug-in runtime or audio-effect checkpoint in this repo. Mamba-3 needs its own dependency/runtime validation. | Run the upstream-supported environment; separately design and test an audio task and export/runtime path. |
| `MambaDAW/src/mamba_layer.*`, `mamba_model.*` | **C++ prototype / in progress** | Custom recurrent SSM layer with input-independent learned `dA`, `dB`, and `C`; not upstream selective Mamba or Mamba-3. A narrow direct C++ layer test is available. | Keep deterministic layer tests; match a specific training/export format; add Python/C++ parity fixtures. |
| `MambaDAW` CLAP path | **In progress; build-blocked** | `plugin_processor.h` includes absent `src/inference_thread.h`; related engine/thread implementation is also absent. The default GUI configuration points at an absent `../JUCE/modules` directory. The wrapper/model loading fragments do not amount to a complete runnable inference path. | Implement or remove the missing path; make a clean configure/build succeed with explicit dependency options; validate with a CLAP host. |
| `clap-plugin/` | **Research/legacy experiment; not real-time safe** | Requires LibTorch and calls Torch/STFT/model operations from the audio callback. The wrapper returns empty initial state; effect decoder weights are initialized randomly. No verified trained Mamba-3 audio model or integration test is present. | Do not ship this path as an audio-thread inference engine. Either retire it or redesign around bounded worker inference and trained decoders. |
| `plugin/` | **Architecture prototype / in progress; not buildable as checked** | CMake lists multiple absent sources (including `MambaInferenceEngine.cpp`, STFT/OLA implementations, and default UI/ARA sources). The `PluginCore` path does not wire the audio/STFT pointers to the worker or consume model results; another CLAP path contains only basic DSP. | Select one wrapper/core path, implement required source files and end-to-end result flow, then build and test it. |
| `mamba-compressor/` | **Python research prototype** | PyTorch model/training code explores Mamba + differentiable DSP. No validated pretrained checkpoint, quality study, real-time plug-in, or measured latency result is established by the audited files. | Run reproducible training/evaluation and document data, checkpoint, baselines, and listening results. |
| LoRA / adapters / RunPod | **Not implemented** | Repository has no verified LoRA training/export, adapter loading, RunPod worker, or customer-facing model pipeline. | Prototype only after a base audio model and train/export/runtime contract work end to end. |
| Plugin formats / distribution | **Unverified** | There is CLAP-oriented source, but no release build or host-validation evidence in this audit. Optional VST3/LV2/GUI options are not proof of supported products; no AAX path is verified. | Build and validate each claimed format independently; document signing/licensing and supported hosts. |
| AI mastering service / API / SaaS designs | **Product concept only** | The PRD, API/data model, test plan, and system-design documents describe proposed workflows; they do not establish a deployed service, validated mastering model, customer demand, pricing, cost, security, or reliability. | Validate the product/problem with users; implement a minimal service only after model quality, licensing, operating costs, and security requirements are evidenced. |

## Chosen near-term focus

For the next integration iteration, use **MambaDAW’s custom C++ SSM core** as the single active prototype because its layer can be compiled and tested independently without LibTorch. This is a scope choice, **not** a claim that the CLAP target builds or that the layer is full Mamba/Mamba-3. Treat `clap-plugin/` as legacy research and `plugin/` as an incomplete architecture experiment until one is explicitly consolidated or retired.

## Evidence recorded so far

- The standalone `MambaDAW/tests/test_mamba_layer.cpp` target can be compiled directly with GCC and `mamba_layer.cpp`; this only verifies narrow layer behavior. The harness now uses deterministic weights, propagates failures, and exits non-zero on failure; the corrected seven-case test passed.
- Static source inventory finds the missing `MambaDAW/src/inference_thread.h` and multiple missing `plugin/` CMake sources.
- Full CMake/Ninja configuration and Python/PyTorch model execution have not been run in this environment because CMake, Ninja, and PyTorch are unavailable.
- The claim-by-claim audit is in [`CLAIMS_VERIFICATION_2026-10.md`](./CLAIMS_VERIFICATION_2026-10.md).

## Release / public-claim gate

Do not claim any of the following until measured and reproducible: “Mamba-3 plug-in,” LoRA support, a useful 60-second context, sub-5-ms/real-time performance, no audio dropouts, production readiness, validated professional audio quality, or predictable freelance revenue. The next release gate is a clean build plus Python/C++ parity, actual host validation, worst-case callback/allocation measurements, and documented listening/quality results.
