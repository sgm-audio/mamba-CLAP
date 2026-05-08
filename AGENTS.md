# Mamba Repository Agent Guidelines

## Setup
- Run setup script: `./scripts/setup-dev.sh` (installs deps, PyTorch, mamba-ssm, dev tools, and DSP dependencies if present)
- Manual setup: 
  1. `pip install -r requirements.txt`
  2. `pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118` (CUDA 11.8)
  3. `MAMBA_FORCE_BUILD=TRUE pip install --no-cache-dir mamba-ssm --no-build-isolation`
  4. `pip install pytest black ruff mypy causal-conv1d>=1.4.0 --no-build-isolation`
  5. For DSP system: `pip install ray numpy hypothesis pytest langsmith neo4j`

## Key Commands
- Basic validation: `python test_mamba.py`
- Run tests: `./scripts/run-tests.sh [all|basic|mamba|dsp]`
  - `all`: Runs test_mamba.py + mamba tests + dsp tests
  - `basic`: Just test_mamba.py
  - `mamba`: pytest in mamba/tests/
  - `dsp`: Tests in dsp_system/
- Build all components: `./scripts/build-all.sh`
- Build C++ plugin (MambaDAW):
  ```bash
  cd MambaDAW
  mkdir build && cd build
  cmake .. -DCMAKE_PREFIX_PATH=/path/to/libtorch
  cmake --build . --config Release
  ```
- Build CLAP plugin: See BUILD_AND_TEST_GUIDE.md (requires CLAP_PATH & TORCH_INSTALL_PATH env vars)
- Activate pre-commit hooks: `pre-commit install`

## Repository Structure
- `/mamba`: Core Mamba-SSM implementation (published as mamba_ssm package)
- `/mamba-compressor`: Context-aware dynamic audio processing application
- `/clap-plugin`: CLAP audio model integration (C++/JUCE)
- `/MambaDAW`: Standalone C++ CLAP host/plugin architecture (LibTorch-based)
- `/dsp_system`: Emotion-aware DSP system with Ray actors and Neo4j coordination
- Standalone Python files: enhanced_mamba_audio.py, nemo_mamba_audio_model.py, etc.
- `/docs`: API references, guides, component documentation
- `/scripts`: Automation scripts for setup, testing, and building

## Important Notes
- Import Mamba as: `from mamba_ssm import Mamba` (or Mamba2/Mamba3)
- CUDA builds require `--no-build-isolation` flag to use existing PyTorch installation
- Testing uses lm-evaluation-harness (`pip install lm-eval==0.4.2`) for some benchmarks
- Requires Linux; NVIDIA GPU recommended for full CUDA acceleration
- Models available on Hugging Face: state-spaces organization
- Development conventions:
  - Python for research/training/orchestration
  - C++/CUDA for high-performance kernels and real-time plugins
  - All DSP layers must maintain gradient path for end-to-end optimization
  - Use type hinting in Python (checked via mypy)

## Action Items & Next Steps

### CRITICAL BLOCKERS (Must Fix Before ANY Production Deployment)

#### 1. Real-Time Audio Thread Allocations
- [ ] **Eliminate ALL heap allocations in process()** – pre-allocate tensors in `activate()`, reuse buffers in `processAudioBlock()`
- [ ] **Verify with valgrind massif** – 0 heap allocations during 60s continuous audio
- [ ] **Remove std::vector from hot loop** – use persistent `MambaState` member
- [ ] **No torch::zeros(), torch::empty(), .clone()** in audio callback

#### 2. Hardcoded Secrets in Production
- [ ] **Remove fallback dev secrets** from `backend/config.py` ProductionConfig
- [ ] **Fail fast** if SECRET_KEY, JWT_SECRET_KEY, DATABASE_URL, REDIS_URL not set in production
- [ ] **Never log secrets** in application startup

#### 3. Job Idempotency & Duplicate Processing
- [ ] **Add idempotency guard** – use database-level lock or check status before processing
- [ ] **Add `processing_worker_id` column** to track which worker is handling job
- [ ] **Atomic claim pattern** – update status 'pending' → 'processing' only if currently pending

#### 4. Job Stuck-State Recovery
- [ ] **Implement recovery daemon** – mark jobs in 'processing' > 30min as 'failed'
- [ ] **Schedule via cron/systemd** every 5 minutes
- [ ] **Send alert** when stuck jobs detected

#### 5. S3 URI Validation Weaknesses
- [ ] **Add length limit** (max 1024 chars)
- [ ] **Reject control characters** (\n, \r, \t)
- [ ] **Canonicalization check** – ensure unquote(key) == key
- [ ] **Test path traversal attacks** against validation

#### 6. Blocking I/O in Audio Path
- [ ] **Remove ALL std::cerr/std::cout** from `processAudioBlock()`
- [ ] **Use atomic logging flags** to defer I/O to NRT thread
- [ ] **No file I/O, printf, exceptions** in audio callback

### FFTW Scaling & Compensation Issues (Already Fixed)
- [x] **CLAP inverse scaling fixed** – changed from `1/n_fft` to `1/√n_fft` in `clap-plugin.cpp:519`
- [x] **MambaDAW inverse scaling** already uses `1/√window_size_` (correct)
- [x] **Window compensation** – multiply output by `1 / sum_squared_window` in both plugins
- [x] **Increased max_block_size_** to 32768 and removed dynamic allocation fallback

#### Data Races & Thread Safety
- [x] **smoothed_dry_wet_ data race** – replaced atomic with mutex in `MambaProcessor`
- [ ] **Verify no other data races** in plugin state (review all shared variables)
- [x] **NaN/Inf check after model inference** (some places done, verify all)

#### S3 & Backend Reliability
- [x] **S3 retry with exponential backoff** – added `retry_s3_operation` to `backend/utils.py`
- [ ] **Filter S3 retry to transient errors only** (ClientError codes: 5xx, Throttling, RequestTimeout)
- [x] **S3 URI validation** – added `validate_s3_uri` in `backend/utils.py`
- [x] **Worker error handling improved** – job marked failed on exceptions
- [ ] **Ensure DB session always closed** – add `finally: db.session.remove()` if missing

#### Security & Rate Limiting
- [x] **Rate limiting on job creation** – 10/min per IP
- [x] **Rate limiting on auth endpoints** – 5/min per IP for register, same for login
- [ ] **Rate limiter supports authenticated users** – use user ID instead of IP when logged in
- [ ] **Account lockout** after 5 failed login attempts (5min lock)
- [x] **Secrets required in production** – `backend/config.py` and `mastering_service/core/config.py` enforce env vars
- [x] **Password hashing method** – added `check_password` to User model
- [ ] **Password complexity** validation during registration
- [ ] **Idempotent job creation** – prevent duplicate charges for same file (check recent pending job)
- [ ] **Token blacklist** for logout (currently token remains valid until expiry)

#### Build & Deployment
- [x] **FFTW dependency added** to CMakeLists for both plugins
- [ ] **Helpful CMake error** if FFTW not found (suggest `apt-get install libfftw3-dev`)
- [ ] **FFTW plan creation failure handling** – catch `std::bad_alloc` and fall back to `FFTW_ESTIMATE`
- [ ] **Validate libfftw3f availability** on all target platforms (Ubuntu/Debian/Fedora/macOS)

#### Audio Quality & correctness
- [x] **FFTW scaling** – forward magnitude divided by √n_fft; inverse also /√n_fft
- [x] **Window compensation** – computed sum_squared_window and applied factor
- [ ] **Verify round‑trip SNR** > 80 dB for FFTW path
- [ ] **Compare RMS with Torch version** – ensure <0.1 dB difference
- [x] **NaN/Inf checks** after model output (CLAP and MambaDAW)
- [ ] **Soft clipping** – already added `std::tanh` in final mix

#### Testing Gaps
- [ ] **Unit tests for AudioFrontend** (round‑trip, scaling)
- [ ] **Unit tests for STFTProcessor** (same)
- [ ] **Latency benchmark suite** (measure worst‑case)
- [ ] **Memory allocation audit** (Valgrind massif)
- [ ] **Integration test for job lifecycle** (backend)
- [ ] **S3 retry unit test** (mocked throttling)
- [ ] **Concurrent job limit test** (race condition)
- [ ] **Rate limiting tests** (per IP and per user)

#### Mamba‑Specific Real‑Time Requirements (State‑Centric Design)
- [ ] **Quantize Mamba model** to INT8/INT4 for CPU real‑time performance (enforce via build)
- [ ] **Profile CPU inference time per frame** – must be < 200 µs for 80‑mel frame on target CPU
- [ ] **Avoid any per‑frame allocations** in Mamba forward pass (reuse hidden state buffers)
- [ ] **Validate state reset** between songs (no memory bleed)
- [ ] **Document expected CPU** (e.g., Intel i7‑12700K, Apple M1) for real‑time operation
- [ ] **Consider kernel fusion** for Mamba SSM step (reduce call overhead)
- [ ] **Cache‑friendly layout** for hidden state (ensure L2 friendly)

#### Code Quality & Maintainability
- [ ] **RAII for FFTW buffers** – use `std::unique_ptr` with custom deleter
- [ ] **Configurable max_block_size** via constructor parameter or env var
- [ ] **CMake options** to optionally disable FFTW and fall back to Torch
- [ ] **Remove dead code** – unused `applyMelSpectrogramEnhancement` in CLAP
- [x] **Const‑correctness** – review getters
- [ ] **Use `std::span`** for buffer references (future C++20)
- [ ] **Structured logging** in C++ (JSON output) for production diagnostics
- [ ] **Add metrics** ( Prometheus counters ) for FFT time, memory, errors

#### Documentation Updates
- [x] **Add FFTW install instructions** to `BUILD_AND_TEST_GUIDE.md`
- [ ] **Document scaling factors** and why we use 1/√n
- [ ] **Document maximum block size** (32768) and how to increase
- [ ] **Describe state‑centric architecture** in README (Mamba state lives on real‑time thread)
- [ ] **Mention quantization requirement** for CPU deployment

#### Operational Considerations
- [ ] **Add monitoring** for job queue depth, worker errors, FFT latency (metrics endpoint)
- [ ] **Graceful degradation** – if model fails repeatedly, auto‑bypass and alert
- [ ] **Alerting** for S3 download/upload failures, DB commit errors
- [ ] **CI pipeline** – include FFTW install and run unit tests on every PR
- [ ] **Docker image** – include libfftw3f and set up non‑root user for security

---

### Priority Order

1. **Build Verification** – ensure FFTW links on all platforms and CMake gives helpful errors.
2. **Correctness** – scaling, compensation, race conditions, S3 retry transient filter.
3. **Security Hardening** – rate limiting per user, idempotency, account lockout, password complexity.
4. **Testing** – unit tests for FFTW wrappers, latency benchmarks, memory audit.
5. **Audio Quality** – round‑trip SNR, RMS comparison, subjective listening.
6. **Performance** – profile CPU, ensure <0.5 ms per callback, no allocations.
7. **Mamba Quantization** – produce quantized model and enforce its use.
8. **Documentation** – update all guides and code comments.
9. **Monitoring & Ops** – metrics, alerts, CI pipeline.
10. **Production Readiness Review** – final sign‑off after all above pass.

---

## Current Status: State-Centric Refactoring (May 2026)

We are in the middle of a critical refactoring to support real-time Mamba-SSM inference on CPU. The previous architecture processed entire STFT blocks inside the audio callback using `torch::Tensor`, causing real-time violations (non-deterministic heap allocation, thread stalls, and state loss on block boundaries).

**Completed (This Session):**
- `mamba_layer.h` / `mamba_layer.cpp` – refactored to raw `std::vector<float>` arrays, real-time safe (`processSample` is allocation-free).
- `mamba_model.h` / `mamba_model.cpp` – refactored to manage `MambaLayer` stack, sample-by-sample processing.
- `plugin_processor.h` – refactored to remove `torch`, `AudioFrontend`, and `ModelLoader` dependencies.

**In Progress / Next Critical Steps:**
- `plugin_processor.cpp` – needs to be rewritten to call `mamba_model_->processSample()` inside the audio callback.
- `CMakeLists.txt` – needs CPU-only build path (hide CUDA behind `option(USE_CUDA)`).
- `MambaWeightLoader` – needs a custom binary weight format to replace `torch::jit::load()`.

**Full Roadmap:**
See `docs/NEXT_STEPS.md` for the complete, detailed roadmap for the next coding agent.

---

**Remember**: Mamba’s state is the memory. Never move state off the real‑time thread. Run quantized inference directly in `processBlock`. Test on target hardware before release.
