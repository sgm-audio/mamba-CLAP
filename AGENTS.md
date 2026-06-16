# Mamba Based — Agent Instructions

## Critical RT Constraints (never violate in `process()`/`processBlock()`)
- NO heap allocations: no `new`, `malloc`, `std::vector::push_back`, `std::string`
- NO mutexes, locks, heavy atomics — use lock-free queues for UI↔DSP
- NO exceptions, file I/O, or logging
- ALL ML inference buffers pre-allocated in `prepare()`/`init()`
- Mamba hidden state `h_t` persists across calls — reset only via `activate()/deactivate()`. Zeroing between blocks causes audio clicks
- Weight loading: main thread only, via `MambaWeightLoader` (mmap, `PROT_READ`)

## Mamba-SSM Statefulness
- `conv_state` and `ssm_state` MUST pass as inputs/outputs between inference runs
- ONNXRuntime C++ API only — no PyTorch/LibTorch in MambaDAW RT path

## Task Ownership

| Task | Owns | Must not touch |
|------|------|----------------|
| DSP Effects | `MambaDAW/src/dsp/*` | inference/feature files |
| Inference Pipeline | `feature_extractor.*`, `inference_thread.*`, `effect_decoder.*` | `src/dsp/` |
| Core Processor | `plugin_processor.*`, CMakeLists `mamba_core` target | format wrappers |
| Plugin Wrappers | `plugin_wrapper*.cpp`, format-specific CMake targets | `src/dsp/`, inference/feature |
| Python Training | `MambaDAW/python_training/` | C++ files |

Tasks A, B, D are parallel. C needs `effect_params.h` + headers from A/B.

## Architecture (MambaDAW CLAP plugin)
```
Audio Thread (RT-safe):  stereo in → FeatureExtractor (STFT→mel, hop=512, N_MEL=80)
                         → pushFrame() → [SPSC feat_queue]
                         ← getLatestParams() ← [seqlock param_double_buffer]
                         → DspChain: EQ → Compressor → Saturator → Reverb → dry/wet → out

Background Thread (non-RT): popFrame() → context ring buffer (60s = 5168 frames @ 44.1k/512)
                            → MambaModel(d_model=80, n_layers=4, d_state=16, d_conv=4) every 128 frames
                            → EffectDecoder → EffectParamBlock (19 AI-predicted floats, sigmoid-clamped)
                            → write [seqlock param_double_buffer]
```
The `EffectParamBlock` struct (`MambaDAW/src/dsp/effect_params.h`) has **21 fields** — AI predicts 19 (`N_EFFECT_PARAMS`); `eq_mid_hz` and `eq_mid_q` are manual-path only. Do NOT duplicate the struct. Default dimensions: `N_MEL=80`, `HOP_SAMPLES=512`, `N_FFT=2048`, `N_EFFECT_PARAMS=19`. Override via cmake `-DMAMBA_N_MEL=...` etc. (see `mamba_config.h.in`).

## Build Commands

### MambaDAW (multi-format CMake project `MambaDAW`)
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

### C++ (MambaDAW) — build then run from `MambaDAW/build`
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
CMake conditionally adds each target when source + dependencies exist. All 12 binaries run in CI.

### Python
```bash
python test_mamba.py                        # quick smoke test
./scripts/run-tests.sh all                  # same as above
./scripts/run-tests.sh mamba                # cd mamba && pytest tests/ -v
./scripts/run-tests.sh dsp                  # cd emotion-aware-dsp && pytest -v
pytest mastering_service/tests/
pytest backend/tests/
```

### Lint / format / typecheck
```bash
pre-commit run --all-files   # black, isort, ruff (--fix --exit-non-zero-on-fix), mypy, gitleaks, trailing-whitespace, end-of-file-fixer, check-yaml, check-added-large-files, check-merge-conflict, check-vcs-permalinks, etc.
```

## CI (`.github/workflows/ci.yml`)

Three parallel jobs:

| Job | Steps |
|-----|-------|
| **build-cpp** | cmake + build + all 12 C++ test binaries (incl. `test_alloc_audit` + `test_tsan`) |
| **python-lint** | `black --check`, `isort --check`, `ruff check` on `mamba/`, `mamba-compressor/`, `MambaDAW/python_training/`, `mastering_service/`, `backend/` |
| **pre-commit** | `pre-commit run --all-files` |

CI does NOT run Python unit tests (no torch/mamba in CI runner).

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

## Known Gotchas
- **CLAUDE.md** and **README.md** reference stale `dsp_system/` dir (actually `emotion-aware-dsp/`). `setup-dev.sh` also checks for `dsp_system/`.
- **`-DENABLE_QUANTIZATION=ON`** does not exist in any CMakeLists. Don't use it.
- Directory `specs/` has ADRs preceding the authoritative `docs/adr/` — prefer `docs/adr/` (ADR-001 through ADR-007). Read these before changing threading/memory.
- After writing audio-thread code, add `// RT-Safety: <justification>` comment at top of function.
- Always provide both `.h` and `.cpp` for new C++ files; update CMakeLists.txt SOURCES.
