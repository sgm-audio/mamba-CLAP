# Mamba Based — Agent Instructions

> **Repository status (2026-10-04): research/prototype; no production-ready plug-in is verified.** Some architecture, build, test, and performance notes below describe intended or historical work rather than existing files. Check [`docs/PROJECT_STATUS.md`](docs/PROJECT_STATUS.md) first. In particular, `MambaDAW/src/inference_thread.h` is absent, the separate `plugin/` target lists missing sources, and no `.github/workflows/ci.yml` is present in this checkout. Do not claim builds/tests/RT guarantees that were not run.

## RT design constraints for future/verified callback code

These are safety targets for new code, not a statement that the current tree complies. The complete plug-in path has not been built or audited. Check the called functions and dependencies before assigning audio-thread work.
- Avoid heap allocation, blocking locks, exceptions, file I/O, and logging in a real-time callback.
- Allocate fixed buffers before processing; verify that wrappers and libraries do not allocate internally.
- Preserve recurrent state only where the model contract requires it; reset behavior must be specified and tested.
- Load weights away from the audio callback. `MambaWeightLoader` exists, but no complete training/export/plugin load path is verified.

## Proposed sequence-model state contract (not integrated)
- A streaming model must explicitly pass its convolution/SSM state between inference steps and define reset/seek behavior.
- The prior ONNX Runtime-only plug-in path was a design proposal; no ONNX audio model/runtime integration is established in `MambaDAW`.

## Task Ownership

| Task | Owns | Must not touch |
|------|------|----------------|
| DSP Effects | `MambaDAW/src/dsp/*` | inference/feature files |
| Inference Pipeline | `feature_extractor.*`, `inference_thread.*`, `effect_decoder.*` | `src/dsp/` |
| Core Processor | `plugin_processor.*`, CMakeLists `mamba_core` target | format wrappers |
| Plugin Wrappers | `plugin_wrapper*.cpp`, format-specific CMake targets | `src/dsp/`, inference/feature |
| Python Training | `MambaDAW/python_training/` | C++ files |

Tasks A, B, D are parallel. C needs `effect_params.h` + headers from A/B.

## Intended architecture (MambaDAW; incomplete and not verified)
```
Proposed audio callback (RT safety unverified): stereo input → feature extraction
                                      → bounded queue / parameter handoff (not implemented end to end)
                                      → conventional DSP → output

Proposed background inference: features → context buffer (60s is a target, not a measured/useful-memory result)
                                → sequence model → parameter decoder → handoff
```
This diagram is intended architecture only. The full feature → queue → inference → parameter path is not currently verified; `inference_thread.h` is absent. `EffectParamBlock` currently has **21 fields** (`MambaDAW/src/dsp/effect_params.h`); do not assume the `N_EFFECT_PARAMS=19`/manual-path split is trained or validated. Check `mamba_config.h.in` and the current implementation before changing dimensions.

## Build Commands

### MambaDAW (intended CMake build; currently blocked)

The following commands are historical/intended, not verified. The CLAP target currently has a missing `inference_thread.h` dependency, and the default GUI setting expects a JUCE checkout at `../JUCE/modules`. Do not promise that these commands produce a plug-in until those blockers are fixed and a clean build succeeds.
```bash
cd MambaDAW && cmake -B build -G Ninja . && cmake --build build -j$(nproc)
# Outputs:
#   MambaDAW/build/Release/MambaClapPlugin.clap    (CLAP, always built)
#   MambaDAW/build/Release/MambaDAW.vst3/...       (VST3, when -DMAMBA_ENABLE_VST3=ON)
#   MambaDAW/build/Release/mambadav.lv2/...        (LV2,  when -DMAMBA_ENABLE_LV2=ON)
cp MambaDAW/build/Release/MambaClapPlugin.clap ~/.clap/
```
`MAMBA_MODEL_PATH` env var required before launching REAPER. No `-DENABLE_QUANTIZATION` flag exists. CLAP SDK fetched via `FetchContent` — do NOT add local clone.

Build options:
- `-DMAMBA_ENABLE_CLAP=ON` (default) — CLAP plugin (`mamba_clap`)
- `-DMAMBA_ENABLE_VST3=ON` — VST3 plugin (`mamba_vst3`, Phase 2 implementation pending)
- `-DMAMBA_ENABLE_LV2=ON` — LV2 plugin (`mamba_lv2`, Phase 3 implementation pending)
- `-DMAMBA_ENABLE_GUI=ON` (default) — JUCE GUI for CLAP plugin
- `-DMAMBA_N_MEL=80` etc. — override model dimensions (see `mamba_config.h.in`)

All format-agnostic code lives in `mamba_core` (STATIC lib). Each wrapper target (`mamba_clap`, `mamba_vst3`, `mamba_lv2`) links `mamba_core`. Tests also link `mamba_core` instead of duplicating sources.

### Legacy clap-plugin (inside distrobox `build-env` only, NOT on host)
```bash
cd clap-plugin && cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DTorch_DIR="$HOME/.local/lib/python3.10/site-packages/torch/share/cmake/Torch" \
  -DCMAKE_CXX_FLAGS="-I/usr/include/python3.10 -D_GLIBCXX_USE_CXX11_ABI=1" && \
cmake --build build -j$(nproc) && cp build/Release/MambaClapPlugin.clap ~/.clap/MambaFX.clap
```
Uses local clone at `clap-plugin/clap-sdk/` — do NOT replace with FetchContent. Known bugs in `clap-plugin/src/clap-plugin.cpp` per `LEGACY_CLAP_PLUGIN.md`.

### Rust crates
```bash
cargo test                    # root mamba_prompt_system crate
cd ara-intelligence-engine && cargo build --release   # → target/release/libara_intelligence_engine.so
```

### Weight export
```bash
cd MambaDAW/tools && python export_weights.py export \
  --checkpoint ../python_training/resources/effect_controller_final.pt \
  --out ../resources/effect_controller.mambaweights && \
python export_weights.py verify ../resources/effect_controller.mambaweights
```
Auto-detects `layers.N.dA` keys from `SimpleMambaModel`. Decoder heads NOT exported — C++ `EffectDecoder` handles sigmoid→range via `kRanges` in `effect_decoder.cpp`.

## Test Commands

### C++ (MambaDAW) — target list, not a verified full suite

Only the standalone `test_mamba_layer.cpp` was compiled and run directly in the current audit. Full CMake targets below are conditional on sources/dependencies and have not been verified; check that each executable exists before running it.
```bash
cd MambaDAW/build && cmake --build .
./test_dsp                  # DSP chain unit tests
./test_inference_thread     # inference pipeline
./test_mamba_layer          # Mamba layer step
./param_gate_test           # parameter gate smoothing
./test_state_serialization  # state ser/deser
./test_latency              # latency measurement
./test_noise_floor          # noise floor measurement
./test_spectral_feedback    # spectral feedback
./test_cpu_budget           # CPU budget monitor
MAMBA_SOAK_SECONDS=10 ./test_soak  # soak test (env var controls duration)
./test_alloc_audit          # RT heap-allocation audit
./test_tsan                 # thread sanitizer concurrency
```
CMake conditionally adds some targets when source files and dependencies exist. The current checkout has no `.github/workflows/ci.yml`, and there is no evidence that 12 binaries are built or run in CI.

### Python tests and quality tools — not verified

Do not treat the legacy Python dispatcher as a verified passing suite. `scripts/run-tests.sh all` invokes the present root `test_mamba.py`, but this audit environment lacks PyTorch so execution fails before tests can be established. The `dsp` target is unavailable because no DSP test component exists in this checkout; it now exits non-zero rather than reporting success. The `mamba` target delegates to vendored upstream tests but has not been run in this audit environment. Service/backend test paths and pre-commit/lint commands must be checked against existing files before use. See [`docs/BUILD_AND_TEST_GUIDE.md`](docs/BUILD_AND_TEST_GUIDE.md) for the one verified test.

## CI status — not configured in this checkout

No `.github/workflows/ci.yml` is present in the current tree. The previous description of build, lint, pre-commit jobs and 12 C++ test binaries is unverified and is withdrawn. Add CI only after a clean build path and its actual test targets are established; report exactly what it runs.

## Python Code Quality
- Formatter: `black --line-length 100`
- Imports: `isort --profile black`
- Lint: `ruff` — excludes `mamba/ssm/cute/`, `mamba/ssm/ops/triton/`, `mamba/ssm/ops/test_`
- Types: `mypy --ignore-missing-imports --follow-imports=silent --explicit-package-bases` — with `additional_dependencies: [types-PyYAML, types-redis]`
- mypy `ignore_errors` (in `mypy.ini`): `backend.models`, `backend.config`, `mamba.mamba_ssm.ops.triton.*`, `mamba.mamba_ssm.ops.tilelang.*`, `mamba.mamba_ssm.ops.cute.*`, `mamba.tests.ops.*`, `mamba.setup`

## Lock-Free Patterns (ADR-003)

| Channel | Mechanism |
|---------|-----------|
| Params → Audio | SPSC ring buffer or aligned `std::atomic<float>` array |
| ARA conditioning → Audio | Double-buffered `ConditioningFrame`; atomically swap index |
| Audio state → Main | Seqlock double-buffer; audio writes, flips index; main reads with retry |

## DSP Formulas (easy to get wrong)
- **Compressor attack/release**: `exp(-1 / (ms * sr / 1000))` — NOT `exp(-1 / (ms / 1000))`
- **FDN reverb per-line feedback gain**: `g = 0.001 ^ (delay_len / (decay_s * sr))` — per delay line, not global
- **EffectDecoder mapping**: sigmoid → scale to range via `kRanges` in `effect_decoder.cpp` (NOT in `effect_params.h`)
- **FFTW scaling** (legacy plugin only): forward ÷ √n_fft; iSTFT OLA ÷ `sum_squared_window ≈ 2/n_fft` (Hann)

## Known documentation/build pitfalls
- Older documents refer to absent `dsp_system/` or `emotion-aware-dsp/` directories. `scripts/setup-dev.sh` checks for `dsp_system/`, and `scripts/run-tests.sh dsp` checks for `emotion-aware-dsp/`; neither path exists in this checkout.
- **`-DENABLE_QUANTIZATION=ON`** does not exist in any CMakeLists. Don't use it.
- `specs/` contains historical ADR drafts; `docs/adr/` is the current design reference, but its entries are proposals and do not prove implementation. Verify all source paths and tests against the current tree.
- When writing code intended for an audio callback, document the constraints and called dependencies, but do not label it RT-safe until allocation/blocking/deadline audits and host tests support that claim.
- Always provide both `.h` and `.cpp` for new C++ files; update CMakeLists.txt SOURCES.
