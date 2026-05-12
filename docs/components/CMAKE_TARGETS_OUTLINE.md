# CMake Targets Outline

## Top-Level Goals
- Keep RT audio path dependency-light and deterministic.
- Separate host integration, ML artifact loading, and UI/state concerns.
- Support CLAP + ARA core with optional wrappers.

## Proposed Targets

1. `mamba_config_generated` (INTERFACE)
- Exposes generated `config.h` include directory.
- Defines compile-time model constants and build guards.

2. `mamba_rt_core` (STATIC)
- Domain: SSM Core.
- Contains inference kernels, state buffers, processing modes.
- RT-safe compile options enabled.
- Forbidden deps in this target: CUDA runtime, BLAS, heavy ML frameworks.

3. `mamba_ml_pipeline` (STATIC)
- Domain: ML Pipeline.
- Contains weight mmap loader, metadata validator, config compatibility checks.
- NRT-only.

4. `mamba_host_api` (STATIC)
- Domain: Host API.
- Contains CLAP lifecycle adapters, param/event bridges, ARA bridge facade.

5. `mamba_ui_state` (STATIC)
- Domain: UI/State.
- Contains parameter model, serialization, preset/session handling.

6. `mamba_clap_plugin` (MODULE)
- Final CLAP plugin binary.
- Links: `mamba_host_api`, `mamba_rt_core`, `mamba_ml_pipeline`, `mamba_ui_state`, `mamba_config_generated`.

7. `mamba_ara_bridge` (STATIC)
- ARA SDK-facing bridge layer (optional if host build excludes ARA).
- Provides context extraction and offline render coordination.

8. `mamba_lv2_wrapper` (MODULE, optional)
- Optional Linux wrapper target.
- Must not alter `mamba_rt_core` contracts.

9. `mamba_logic_wrapper` (MODULE, optional)
- Wrapper/bridge path for Logic compatibility if CLAP wrapper is required.

## Build Options
- `option(MAMBA_ENABLE_ARA ON)`
- `option(MAMBA_ENABLE_LV2_WRAPPER OFF)`
- `option(MAMBA_ENABLE_LOGIC_WRAPPER OFF)`
- `option(MAMBA_RT_ASSERTS ON)`
- `option(MAMBA_BENCHMARKS ON)`

## Compile and Link Rules
- C++ standard: `CXX_STANDARD 20`.
- `mamba_rt_core` uses strict warning flags and disables exceptions in RT translation units where feasible.
- Symbol visibility restricted on plugin modules.
- Platform-specific fat/universal handling:
  - macOS: universal slices configured at top-level toolchain profile.
  - Windows: target Win10+ APIs.
  - Linux: optional LV2 wrapper build path.

## Performance Gate Targets
1. `mamba_perf_default_model` (test/bench)
- Verifies <= 30% RT budget on reference CPU profile.

2. `mamba_rt_alloc_audit` (test)
- Fails if allocations detected in audio callback loop.

3. `mamba_state_roundtrip_test` (test)
- Validates serialization of params + weight path + base64 `h_state` snapshot.
