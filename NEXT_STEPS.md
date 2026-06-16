# MASTER IMPLEMENTATION ROADMAP: Production Readiness

**Created:** 2026-05-08
**Reviewer:** Production Code Review Agent
**Critical Status:** ⚠️ **PRODUCTION READINESS: FAILED** - Multiple critical blockers must be fixed
**Estimated Critical Path:** 40-60 hours (1-2 weeks with 1 engineer)

---

## EXECUTIVE SUMMARY

The Mamba repository combines state-of-the-art Mamba-SSM models with professional audio processing. The codebase shows strong research foundations but **lacks production-grade engineering discipline**. Critical issues exist in **real-time audio safety**, **security**, **reliability**, and **testing** that would cause crashes, security breaches, or poor user experience in production.

**Key Findings**:
- 🔴 **6 CRITICAL BLOCKERS** (must fix before any production deployment)
- 🟡 **12 HIGH-PRIORITY ISSUES** (address immediately after blockers)
- 🟢 **9 MEDIUM/LOW IMPROVEMENTS** (polish and maintainability)
- ❌ **CI/CD NOT ESTABLISHED** - No automated testing or quality gates
- ⚠️ **ARCHITECTURAL CONCERNS** - Duplicate plugin implementations, incomplete real-time safety

---

## PHASE 0: IMMEDIATE RISK MITIGATION (Day 1-2)

### Task 0.1: Disable Broken Plugin Build (if needed)
If the current CLAP plugin cannot pass real-time safety validation, create a build flag to prevent shipping:
- Add `-DENABLE_REALTIME_CHECKS=ON` to CMake
- Fail build if `valgrind` detects allocations in process()
- Document that production builds require `RT_DEBUG=0` and zero allocations

---

## PHASE 1: REAL-TIME SAFETY REFACTORING (Week 1-2) - CRITICAL

### Task 1.1: Eliminate All Heap Allocations in Audio Callback

**Files**: `clap-plugin/src/clap-plugin.cpp`, `MambaDAW/src/plugin_processor.cpp`

**Problem**: Current `processAudioBlock()` allocates memory on every callback:
```cpp
auto input_tensor = torch::from_blob(...).clone();  // MALLOC
torch::Tensor processed_mel = torch::zeros_like(mel_spec);  // MALLOC
std::vector<torch::Tensor> state;  // MALLOC in loop
```

**Impact**: At 44.1kHz with 256-sample blocks, you have 5.8ms budget. A 1ms allocation causes dropout.

**Solution - Pre-allocate Everything**:

1. Add to `MambaClapPlugin` class members:
```cpp
// Pre-allocated buffers (allocated in activate(), freed in deactivate())
torch::Tensor prealloc_input_;       // [1, MAX_HOP=2048]
torch::Tensor prealloc_mel_spec_;    // [MAX_FRAMES, n_mels=80]
torch::Tensor prealloc_processed_mel_; // [MAX_FRAMES, n_mels]
torch::Tensor prealloc_effect_params_; // [MAX_FRAMES, 4]
std::vector<torch::Tensor> persistent_state_; // Mamba state (pre-allocated)
```

2. Refactor `activate()`:
```cpp
bool MambaClapPlugin::activate(double sample_rate, ...) {
    is_active_ = true;
    sample_rate_ = static_cast<int>(sample_rate);

    // Pre-allocate ALL tensors (real-time safe)
    const int MAX_HOP = 2048;
    const int MAX_FRAMES = 9; // for 2048/256 hop

    prealloc_input_ = torch::empty({1, MAX_HOP}, torch::kFloat32);
    prealloc_mel_spec_ = torch::empty({MAX_FRAMES, n_mels_}, torch::kFloat32);
    prealloc_processed_mel_ = torch::empty({MAX_FRAMES, n_mels_}, torch::kFloat32);
    prealloc_effect_params_ = torch::empty({MAX_FRAMES, 4}, torch::kFloat32);

    // Allocate persistent Mamba state (persists across blocks)
    persistent_state_ = mamba_wrapper_->getInitialState(1); // batch=1

    // Ensure STFTProcessor is created (it pre-allocates FFTW buffers)
    if (!stft_processor_) {
        stft_processor_ = std::make_unique<STFTProcessor>(n_fft_, hop_length_, n_mels_, sample_rate_);
    }

    return true;
}
```

3. Refactor `processAudioBlock()`:
```cpp
bool MambaClapPlugin::processAudioBlock(const float* input, float* output, int num_samples) {
    // 1. Copy dry signal to output (memcpy only - RT safe)
    std::memcpy(output, input, num_samples * sizeof(float));

    if (!mamba_wrapper_ || !model_loaded_) {
        return true; // Dry only
    }

    try {
        torch::NoGradGuard no_grad;

        // 2. Copy input to pre-allocated tensor (NO allocation)
        float* input_data = prealloc_input_.data_ptr<float>();
        std::memcpy(input_data, input, num_samples * sizeof(float));

        // 3. STFT forward (uses pre-allocated FFTW buffers internally)
        int T = 0;
        try {
            T = stft_processor_->forward(
                prealloc_input_.slice(0, 0, num_samples),
                prealloc_mel_spec_
            );
        } catch (const std::exception& e) {
            // Log error (deferred to NRT) but continue with dry signal
            return true;
        }

        // 4. Mamba recurrent processing (stateful, must NOT allocate)
        // Use shallow copy of state tensor wrappers (they share underlying storage)
        auto state = persistent_state_;

        for (int t = 0; t < T; ++t) {
            // Get mel frame as tensor VIEW (no copy, no allocation)
            auto mel_frame = prealloc_mel_spec_.slice(0, t, t+1); // [1, n_mels]

            // Call quantized step - MUST be allocation-free
            torch::Tensor effect_params_frame;
            mamba_wrapper_->step_quantized(mel_frame, state, effect_params_frame);

            // Store effect parameters in preallocated buffer
            if (effect_params_frame.numel() == 1) {
                float param = effect_params_frame.item<float>();
                prealloc_effect_params_[t][getEffectTypeIndex()] = param;
            }
        }

        // Persist updated state for next audio block
        persistent_state_ = state;

        // 5. Apply effect modulation to mel spectrogram
        for (int t = 0; t < T; ++t) {
            float effect_strength = prealloc_effect_params_[t].mean().item<float>();
            float modulation = 1.0f + (effect_strength - 0.5f) * 2.0f; // [0,1] -> [0,2]

            auto mel_frame = prealloc_processed_mel_.slice(0, t, t+1);
            mel_frame *= modulation; // in-place scaling (no new allocation)
        }

        // 6. ISTFT back to waveform (uses pre-allocated FFTW buffers)
        torch::Tensor output_tensor;
        try {
            output_tensor = stft_processor_->inverse(
                prealloc_processed_mel_.slice(0, 0, T)
            );
        } catch (const std::exception& e) {
            return true; // dry signal already present
        }

        // 7. Mix dry/wet (use pre-allocated output tensor memory)
        const float* wet_data = output_tensor.data_ptr<float>();
        float mix = param_mix_.load(std::memory_order_relaxed);

        for (int i = 0; i < num_samples; ++i) {
            float wet = wet_data[i];
            // Sanitize (NaN/Inf protection)
            if (std::isnan(wet) || std::isinf(wet)) wet = 0.0f;
            float dry = output[i];
            float mixed = dry * (1.0f - mix) + wet * mix;
            // Soft clip to prevent overflow
            output[i] = std::tanh(mixed);
        }

    } catch (const std::exception& e) {
        // Any exception - output remains dry (already copied)
        // Error deferred to NRT thread for logging
    }

    return true;
}
```

**Critical**: `MambaWrapper::step_quantized()` must also be allocation-free. See Task 1.3.

**Verification**:
```bash
# Build with RT_DEBUG=ON to track allocations
cmake -DCMAKE_BUILD_TYPE=Debug -DRT_DEBUG=ON ..
./scripts/validate-realtime.sh  # (create this script)
# Must report: 0 heap allocations in processAudioBlock() for 60s continuous audio
```

**Acceptance Criteria**:
- ✅ Valgrind massif shows 0 heap allocations in `processAudioBlock()`
- ✅ All `torch::zeros()`, `torch::empty()`, `torch::clone()`, `std::vector` operations removed from audio path
- ✅ Processing time < 80% of block budget (e.g., < 4.6ms for 256@44.1kHz)

---

### Task 1.2: Implement Quantized INT8 Model Pipeline

**Files**: `scripts/quantize_model.py`, `clap-plugin/src/mamba_wrapper.cpp`

**Background**: FP32 Mamba is too slow for CPU real-time. Need INT8 quantization to achieve < 500µs per step.

**Actions**:

1. **Create quantization script** `scripts/quantize_model.py`:
```python
#!/usr/bin/env python3
import torch
import sys

def quantize_mamba_model(input_pt, output_pt):
    # Load original model
    model = torch.jit.load(input_pt)
    model.eval()

    # Quantize to INT8
    model_quantized = torch.quantization.quantize_dynamic(
        model,
        {torch.nn.Linear},  # Quantize linear layers
        dtype=torch.qint8
    )

    # Test accuracy
    example = torch.randn(1, 256, 80)  # [batch, frames, n_mels]
    with torch.no_grad():
        out_fp32 = model(example)
        out_quant = model_quantized(example)
        mse = torch.mean((out_fp32 - out_quant) ** 2)
        print(f"Quantization MSE: {mse.item():.6f}")

    # Save
    torch.save(model_quantized.state_dict(), output_pt)
    print(f"Saved quantized model: {output_pt}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: quantize_model.py <input.pt> <output.pt>")
        sys.exit(1)
    quantize_mamba_model(sys.argv[1], sys.argv[2])
```

2. **Extract quantized weights to C++ structures** in `MambaWrapper`:
```cpp
struct QuantizedLinear {
    const int8_t* weight_q;   // INT8 weights [out_features, in_features]
    const float* scale;       // per-output-channel scale factors
    const float* zero_point;  // zero points
    const float* bias;        // FP32 bias (if present)
    int out_features, in_features;
};

class MambaWrapper {
private:
    std::vector<QuantizedLinear> input_proj_weights_;  // One per layer
    std::vector<QuantizedLinear> out_proj_weights_;
    std::vector<QuantizedLinear> effect_decoder_weights_;

    // Scratch buffer for dequantized intermediate results
    torch::Tensor scratch_buffer_;

public:
    bool loadQuantizedModel(const std::string& path) {
        // Load TorchScript quantized model
        auto model = torch::jit::load(path);
        model.eval();

        // Extract quantized weights from all Linear modules
        // Traverse model.named_modules() and copy quantized params
        // ...

        // Allocate scratch buffer
        scratch_buffer_ = torch::empty({d_inner_}, torch::kFloat32);

        return true;
    }
};
```

3. **Write INT8 matrix-vector multiply kernel**:
```cpp
// quantized_ops.cpp
void quantized_linear_forward_int8(
    const float* input,  // [in_features]
    const int8_t* weight_q,  // [out_features, in_features]
    const float* scale,     // [out_features]
    const float* zero_point, // [out_features] or scalar
    const float* bias,      // [out_features]
    float* output)          // [out_features]
{
    for (int o = 0; o < out_features; ++o) {
        int32_t accum = 0;
        const int8_t* w_row = weight_q + o * in_features;
        for (int i = 0; i < in_features; ++i) {
            accum += (static_cast<int32_t>(w_row[i]) - static_cast<int32_t>(zero_point[o]))
                     * static_cast<int32_t>(input[i]);
        }
        // Dequantize: (accum * scale[o]) + bias
        float result = static_cast<float>(accum) * scale[o];
        if (bias) result += bias[o];
        output[o] = result;
    }
}
```

**Verification**:
```bash
# Benchmark step time
python -c "
import time
import torch
model = torch.jit.load('mamba3_quantized.pt')
example = torch.randn(1, 1, 80)  # single frame
state = None
times = []
for i in range(1000):
    start = time.perf_counter()
    with torch.no_grad():
        out, state = model.step(example, state)
    times.append((time.perf_counter() - start) * 1e6)
print(f'Average: {np.mean(times):.1f} µs (target: <500 µs)')
"
```

**Acceptance**:
- ✅ Quantized model loads successfully in C++
- ✅ Single frame inference < 500 µs on target CPU (i7-12700K or M1)
- ✅ Quantization MSE < 0.001 on validation set
- ✅ Model size reduced by >75% (FP32 ~100MB → INT8 ~25MB)

---

### Task 1.3: Write Fused Selective Scan CPU Kernel

**File**: `mamba_ssm/ops/cpu/selective_scan_fp32.cpp`

**Why**: The naive loop in `MambaLayer::discrete_state_update()` (lines 66-94 of mamba_layer.cpp) is ~100x too slow for real-time. Each time step loops over `d_inner * d_state` with scalar operations. Needs SIMD vectorization.

**Implementation**:
```cpp
#include <immintrin.h>  // AVX2/AVX-512
#include <cmath>

// Process d_inner channels in parallel
// u: [d_inner] - input (gated after conv)
// state: [d_inner, d_state] - persistent state (updated in-place)
// A_log, B, C, D, dt: model parameters (FP32)
void selective_scan_cpu_fp32(
    const float* __restrict u,  // [d_inner]
    float* __restrict__ state,  // [d_inner, d_state] in/out
    const float* __restrict__ A_log,
    const float* __restrict__ B,
    const float* __restrict__ C,
    const float* __restrict__ D,
    const float* __restrict__ dt,
    int d_inner,
    int d_state) {

    constexpr float kDenormalCutoff = 1e-30f;
    constexpr float kDenormalReplace = 0.0f;

    // Process d_inner in chunks of 8 (AVX2) or 16 (AVX-512)
    for (int i = 0; i < d_inner; i += 8) {
        // Load u[i..i+7]
        __m256 u_vec = _mm256_loadu_ps(u + i);

        // For each i, update all d_state elements (inner loop)
        for (int j = 0; j < d_state; ++j) {
            size_t idx = i * d_state + j;

            // Load state[i, j]
            float state_ij = state[idx];

            // Discretize A: dA = exp(dt * A_log)
            float A_ij = A_log[idx];
            float dA = std::exp(dt[i] * A_ij);  // TODO: precompute if dt constant

            // Update state: state' = dA * state + (dt * B) * u
            float B_ij = B[idx];
            float dB = dt[i] * B_ij;

            // Update with denormal protection
            state[idx] = dA * state_ij + dB * u_vec[0];
            if (std::abs(state[idx]) < kDenormalCutoff) {
                state[idx] = kDenormalReplace;
            }
        }
    }

    // Output: y = C @ state + D * u
    // Initialize output to zero then accumulate
    std::memset(const_cast<float*>(u), 0, d_inner * sizeof(float));  // reuse u as output

    for (int j = 0; j < d_state; ++j) {
        for (int i = 0; i < d_inner; i += 8) {
            // Load 8 state values for this j
            __m256 state_vec = _mm256_loadu_ps(state + i * d_state + j);
            // Load C[j, i..i+7]
            __m256 C_vec = _mm256_loadu_ps(C + j * d_inner + i);
            // Fused multiply-add: u[i..i+7] += C_vec * state_vec
            __m256 acc = _mm256_loadu_ps(u + i);
            acc = _mm256_fmadd_ps(C_vec, state_vec, acc);
            _mm256_storeu_ps(u + i, acc);
        }
    }

    // Add skip connection: y += D * u_input (but u was overwritten, so need original)
    // Handle separately
}
```

**Optimization Notes**:
- Precompute `exp(dt * A_log)` if `dt` constant across frames (cache it)
- Use `__restrict__` to help compiler alias analysis
- Consider AVX-512 if target CPU supports (wide vectors, better throughput)
- Aim for < 50 µs for d_inner=1024, d_state=16 on target CPU

**Acceptance**:
- ✅ Benchmark: selective_scan_cpu_fp32() executes in < 50 µs for typical Mamba-3 dimensions
- ✅ Vectorized code uses at least 80% of CPU's SIMD lanes (profile with perf)
- ✅ Output matches reference Torch implementation (MSE < 1e-5)

---

### Task 1.4: Remove All std::cerr/std::cout from Audio Processing Path

**Files**: `clap-plugin/src/clap-plugin.cpp`, `MambaDAW/src/plugin_processor.cpp`

**Problem**: I/O operations can block (mutex, disk) and cause audio glitches.

**Solution**: Replace with atomic flag → NRT logging:

```cpp
// In MambaClapPlugin.h:
class MambaClapPlugin {
private:
    enum LogPriority { LOG_INFO, LOG_WARN, LOG_ERROR };
    struct LogMessage {
        std::atomic<bool> pending{false};
        std::array<char, 256> buffer;
    };

    LogMessage log_msg_[3];  // Info, warn, error buffers
    std::atomic<bool> log_flush_needed_{false};

public:
    void flushLogsToStderr() {
        // Called from NRT context (e.g., deactivate, host callback)
        for (auto& msg : log_msg_) {
            if (msg.pending.exchange(false)) {
                std::cerr << msg.buffer.data() << std::endl;
            }
        }
        log_flush_needed_ = false;
    }
};

// In processAudioBlock() (on error):
if (std::isnan(wet)) {
    if (!log_msg_[LOG_ERROR].pending.load()) {
        int len = snprintf(log_msg_[LOG_ERROR].buffer.data(),
                          log_msg_[LOG_ERROR].buffer.size(),
                          "[ERROR] NaN in output at frame %d", t);
        log_msg_[LOG_ERROR].pending.store(true);
        log_flush_needed_.store(true);
    }
}

// Host should call plugin->flushLogs() periodically (NRT)
```

**Simpler alternative**: Remove all logging in production builds:
```cpp
#ifdef DEBUG
    #define RT_LOG(...) do { if (!is_audio_thread_) std::cerr << __VA_ARGS__; } while(0)
#else
    #define RT_LOG(...) ((void)0)
#endif
```

**Acceptance**:
- ✅ No `std::cerr`, `std::cout`, `printf`, `fprintf` in `processAudioBlock()` or any inline functions it calls
- ✅ Static analysis (clang-tidy `bugprone-printf-format`) passes

---

### Task 1.5: Fix Hardcoded Secrets in Backend Configuration

**File**: `backend/config.py`

**Current problem**:
```python
class ProductionConfig(Config):
    SECRET_KEY = os.environ.get('SECRET_KEY') or 'dev-secret-key-change-in-production'  # UNSAFE
    # Same for JWT_SECRET_KEY - if env var missing, uses weak default
```

**Fix**:
```python
class ProductionConfig(Config):
    SECRET_KEY = os.environ.get('SECRET_KEY')
    JWT_SECRET_KEY = os.environ.get('JWT_SECRET_KEY')
    SQLALCHEMY_DATABASE_URI = os.environ.get('DATABASE_URL')
    REDIS_URL = os.environ.get('REDIS_URL')

    def __init__(self):
        missing = []
        for var in ['SECRET_KEY', 'JWT_SECRET_KEY', 'DATABASE_URL', 'REDIS_URL']:
            if not getattr(self, var, None):
                missing.append(var)
        if missing:
            raise ValueError(f"Missing required environment variables: {', '.join(missing)}")
```

**Acceptance**:
- ✅ Application fails to start in production mode if any secret missing
- ✅ Error message clearly lists which environment variables are required
- ✅ No defaults in ProductionConfig

---

## PHASE 2: SECURITY & RELIABILITY (Week 2-3) - HIGH PRIORITY

### Task 2.1: Add Idempotency to Job Processing

**Files**: `backend/worker.py`, `backend/models.py` (add column)

**Problem**: If RQ retries or job manually re-queued, same job could be processed twice, deducting credit once but potentially overwriting output.

**Solution**:

1. **Add to Job model**:
```python
# backend/models.py
class Job(db.Model):
    # ... existing fields ...
    processing_worker_id = db.Column(db.String(100), nullable=True)  # Worker identifier
    processing_started_at = db.Column(db.DateTime(timezone=True), nullable=True)
```

2. **Add migration** (use Flask-Migrate):
```bash
flask db migrate -m "Add processing_worker_id to Job"
flask db upgrade
```

3. **Atomic claim in worker**:
```python
def process_mastering_job(job_id: str) -> None:
    app = get_app()
    with app.app_context():
        try:
            job_uuid = uuid.UUID(job_id)
        except ValueError:
            logger.error(f"Invalid job ID: {job_id}")
            return

        # ATOMIC claim: only update if status='pending' (SELECT ... FOR UPDATE)
        job = Job.query.filter_by(id=job_uuid, status='pending').with_for_update().first()
        if not job:
            logger.warning(f"Job {job_id} already processing or completed")
            return

        # Claim it
        job.status = 'processing'
        job.startedAt = datetime.utcnow()
        job.processing_worker_id = f"{socket.gethostname()}:{os.getpid()}"

        try:
            db.session.commit()
        except SQLAlchemyError as e:
            logger.error(f"Failed to claim job: {e}")
            db.session.rollback()
            return

        # ... rest of processing ...
```

**Acceptance**:
- ✅ Two workers cannot process same job simultaneously (database constraint)
- ✅ Job status transitions: pending → processing → (completed|failed)
- ✅ Worker ID tracked for debugging

---

### Task 2.2: Implement Job Stuck-State Recovery

**Files**: New `backend/recovery.py`, systemd timer or cron job

**Problem**: If worker crashes while processing, job remains `processing` forever.

**Solution**:
```python
# backend/recovery.py
#!/usr/bin/env python3
"""Recover stuck jobs (orphaned processing state)."""
import logging
from datetime import datetime, timedelta
from backend.app import create_app, db
from backend.models import Job

logger = logging.getLogger(__name__)

def recover_stuck_jobs(timeout_minutes: int = 30) -> int:
    """Mark jobs stuck in 'processing' for > timeout as 'failed'."""
    app = create_app('production')
    with app.app_context():
        cutoff = datetime.utcnow() - timedelta(minutes=timeout_minutes)

        stuck_jobs = Job.query.filter(
            Job.status == 'processing',
            Job.startedAt < cutoff
        ).all()

        for job in stuck_jobs:
            logger.warning(
                f"Recovering stuck job {job.id} "
                f"(worker: {job.processing_worker_id}, started: {job.startedAt})"
            )
            job.status = 'failed'
            job.errorMessage = f'Worker timeout after {timeout_minutes} minutes'
            job.completedAt = datetime.utcnow()

        db.session.commit()
        logger.info(f"Recovered {len(stuck_jobs)} stuck jobs")
        return len(stuck_jobs)

if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    count = recover_stuck_jobs()
    exit(0 if count >= 0 else 1)
```

**Schedule** (cron):
```cron
*/5 * * * * cd /app && python backend/recovery.py >> /var/log/mamba-recovery.log 2>&1
```

**Monitoring**: Alert if any jobs recovered (Prometheus metric `jobs_recovered_total`).

**Acceptance**:
- ✅ Orphaned jobs auto-recovered within 5 minutes
- ✅ Alert triggered on recovery event
- ✅ No manual intervention required

---

### Task 2.3: Strengthen S3 URI Validation

**File**: `backend/utils.py`

**Current gaps**: No length limit, weak path traversal check, no control character rejection.

**Fix**:
```python
def validate_s3_uri(uri: str) -> Tuple[str, str]:
    if not uri.startswith('s3://'):
        raise ValueError("Invalid S3 URI: must start with 's3://'")

    path = uri[5:]
    if '/' not in path:
        raise ValueError("Invalid S3 URI: missing key after bucket")

    bucket, key = path.split('/', 1)

    # Validate bucket (AWS rules)
    if not re.match(r'^[a-z0-9][a-z0-9\-.]{1,61}[a-z0-9]$', bucket):
        raise ValueError(f"Invalid bucket name")

    # Key validation
    if len(key) > 1024:
        raise ValueError("S3 key too long (max 1024)")

    if any(ord(c) < 32 for c in key):  # control chars
        raise ValueError("S3 key contains control characters")

    if '..' in key or '/./' in key or '//' in key or key.startswith('/'):
        raise ValueError("Path traversal attempt")

    # Canonicalization: ensure decoded == original (prevents %2e%2e%2f etc)
    import urllib.parse
    if urllib.parse.unquote(key) != key:
        raise ValueError("URL encoding not allowed in S3 key")

    return bucket, key
```

**Acceptance**:
- ✅ Fuzzing with path traversal payloads (../, %2e%2e%2f, double-encode) all rejected
- ✅ Long keys (>1024) rejected
- ✅ Control characters (\n, \r, \t) rejected

---

### Task 2.4: Add Rate Limiting to All Auth Endpoints

**File**: `backend/api/v1.py`

**Current**: Only `register` has `@rate_limit(5)`. `login` has none.

**Fix**:
```python
@api_bp.route('/auth/login', methods=['POST'])
@rate_limit(10)  # 10 per minute per IP
def login():
    # ... existing code ...

@api_bp.route('/auth/logout', methods=['POST'])
@jwt_required()
@rate_limit(10)  # Also limit logout (prevent token spam)
def logout():
    # ...
```

**Acceptance**:
- ✅ Login limited to 10/min/IP
- ✅ Returns 429 + Retry-After header when exceeded
- ✅ Rate limits bypassed in DEBUG mode (configurable)

---

## PHASE 3: TESTING & CI (Week 3-4) - HIGH PRIORITY

### Task 3.1: Write API Integration Tests

**Files**: New `backend/tests/test_api_endpoints.py`

**Coverage**:
- Auth: register, login, logout
- Jobs: create, list, get, cancel, result
- Presets, health

**Example**:
```python
def test_create_job_success(client, test_user):
    response = client.post('/api/v1/jobs', json={
        'fileName': 'test.wav',
        'fileSize': 1024,
        'presetId': 'transparent',
        'contentType': 'audio/wav',
        'intensity': 50
    }, headers={'Authorization': f'Bearer {test_user.token}'})

    assert response.status_code == 202
    data = response.json
    assert 'id' in data
    assert data['status'] == 'pending'

def test_create_job_missing_field(client, test_user):
    response = client.post('/api/v1/jobs', json={
        'fileName': 'test.wav',
        # missing fileSize, presetId, contentType
    }, headers={'Authorization': f'Bearer {test_user.token}'})

    assert response.status_code == 400
    assert response.json['error'] == 'VALIDATION_ERROR'

def test_rate_limiting(client):
    # Without auth, rate limit applies to IP
    for i in range(11):
        response = client.post('/api/v1/auth/register', json={
            'email': f'test{i}@example.com',
            'password': 'password123',
            'fullName': 'Test'
        })
        if i >= 10:
            assert response.status_code == 429
```

**Acceptance**:
- ✅ pytest-cov shows >80% coverage for `backend/api/`
- ✅ All error paths tested
- ✅ Rate limiting tests pass

---

### Task 3.2: Add GitHub Actions CI

**File**: `.github/workflows/ci.yml`

**Jobs**:
- `test-backend`: Run pytest with Postgres + Redis services
- `build-clap-plugin`: Try to build plugin (may fail if missing CLAP, but should try)
- `security-scan`: Trivy + Gitleaks

**Acceptance**:
- ✅ CI runs on every PR
- ✅ Build and test status shown on PR
- ✅ Security scan blocks merge on critical vulnerabilities

---

## PHASE 4: ARCHITECTURE DECISIONS (Week 4)

### Task 4.1: Resolve Plugin Duplication

**Observation**: Both `clap-plugin/` and `MambaDAW/` are CLAP plugins with similar functionality.

**Decision** (choose one):
1. **Consolidate**: Merge best of MambaDAW into clap-plugin, deprecate MambaDAW
2. **Specialize**: Keep both but document distinct purposes (CLAP vs standalone host)

**Recommended**: Option 1 - clap-plugin is the main product; MambaDAW was experimental prototype.

**Actions**:
- Merge MambaDAW's better state management into clap-plugin
- Delete or mark MambaDAW as DEPRECATED
- Update docs, build scripts, README

---

## SUCCESS CRITERIA

Before any production deployment, **all P0 (Critical)** tasks must be complete. The codebase must pass:

### Hard Requirements
- ✅ Zero heap allocations in processAudioBlock (valgrind verified)
- ✅ No secrets hardcoded in production config
- ✅ Job idempotency and stuck-state recovery implemented
- ✅ S3 validation passes security audit
- ✅ No I/O operations in audio callback
- ✅ Quantized model loads and runs < 500µs per frame
- ✅ Selective scan kernel fused and SIMD-optimized

### Soft Requirements (Should Have)
- ✅ API integration test coverage >80%
- ✅ CI pipeline set up
- ✅ Rate limiting on all auth endpoints
- ✅ Structured logging in backend
- ✅ Health checks actually verify dependencies

---

## HANDOFF

**Next Agent Should**:
1. Start with **Task 1.1** (eliminate allocations) - this is the hardest and most critical
2. Use `RT_DEBUG=ON` to track allocations during development
3. Benchmark continuously: `./scripts/benchmark-latency.sh` (create)
4. Keep this plan updated as tasks are completed

**Do NOT deploy** until all P0 tasks are done and verified on target hardware.

---

**End of Roadmap** - Total estimated: 140-180 hours across all phases.
- **Requirement:** Build systems must have `libfftw3f` (single-precision FFTW) installed.
- **Ubuntu/Debian:** `apt-get install libfftw3-dev`
- **Fedora/RHEL:** `dnf install fftw-devel`
- **macOS:** `brew install fftw`
- **CMake:** Both `CMakeLists.txt` now call `find_package(FFTW3 REQUIRED COMPONENTS fftw3f)`. Verify this finds the library.

### 2. Compilation Errors
- Rebuild both plugins with CMake in Release mode:
  ```bash
  cd MambaDAW && mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build . --config Release
  cd ../../
  cd clap-plugin && mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build . --config Release
  ```
- Ensure there are **no linker errors** related to `fftwf_*` symbols.
- If FFTW is not found, implement fallback: either build FFTW from source or provide a clear error message instructing the user to install it.

### 3. Numerical Scaling Validation
- The FFTW implementation matches `torch.stft(normalized=True)` scaling:
  - Forward magnitude scaled by `1/√n_fft`
  - Inverse scaled by `1/√n_fft`
- **Must verify** that the model's input distribution (mel-spectrograms) is within expected range. Run a few audio files through both old and new versions and compare mel statistics (mean, std). Differences should be <1%.

---

## High-Priority Tasks (Before First Production Deploy)

### 4. Unit Tests for FFTW Wrappers
Create new test files:
- `MambaDAW/tests/test_audio_frontend_fftw.cpp`
- `clap-plugin/tests/test_stft_processor.cpp`

**Test cases:**
- **Round-trip SNR:** Generate random white noise, process through STFT → iSTFT, compute SNR > 80 dB.
- **Scaling equivalence:** Compare magnitude output of `AudioFrontend`/`STFTProcessor` with `torch.stft(..., normalized=True)` on identical input. Max absolute error < 1e-5.
- **Edge case lengths:** Test with non‑integer multiples of hop size (e.g., 1005 samples with hop=256).
- **Empty input:** Handle zero‑length buffers gracefully.
- **NaN/Inf protection:** Inject NaN into input; verify plugin does not crash and outputs clean signal.

### 5. Latency Benchmarking
- Create a benchmark harness that calls `processBlock`/`processAudioBlock` with typical block sizes (128, 256, 512, 1024) at various sample rates (44.1kHz, 48kHz, 96kHz).
- Measure **99th percentile latency** (worst‑case) using `std::chrono::high_resolution_clock`.
- **Target:** Total processing time < 10% of the callback budget (e.g., < 0.5 ms for 256‑sample block at 44.1kHz).
- Record results in `docs/performance_benchmarks.md`.

### 6. Memory Allocation Audit
- Run Valgrind's `massif` or similar to check for heap allocations during audio processing.
- **Must show:** No allocations above the initial setup (after `activate()` returns).
- If any allocations are found in the hot path (e.g., `std::vector` growth, `new` in loops), eliminate them by pre‑allocating or reusing buffers.

### 7. Audio Quality Validation
- Select a reference audio file (e.g., 10 s of speech + music).
- Process with **old Torch‑based version** (commit before refactor) and **new FFTW version**.
- Compute:
  - RMS level difference (dB)
  - LUFS (integrated) difference
  - Spectral envelope comparison (e.g., 1/3‑octave smoothed spectrum)
- **Acceptance criteria:** RMS diff < 0.1 dB, no audible artifacts (subjective listening test).
- Document results in `docs/audio_quality_validation.md`.

### 8. Update Build & Test Guide
- In `BUILD_AND_TEST_GUIDE.md`, add a section **"Installing FFTW"** with platform‑specific instructions.
- Document the scaling changes and why they are needed.
- Note the maximum supported block size (default 8192 samples). Explain how to increase `max_block_size_` in `STFTProcessor` if needed.
- Add a troubleshooting section for common CMake errors (`FFTW3_NOT_FOUND`).

### 9. Rate Limiting & Concurrency in Backend
- Verify the Flask rate limiting works with Redis. Test by sending 11 requests in 1 minute; expect 429 on the 11th.
- Verify concurrent job limit: create 6 jobs with a user (limit 5); 6th should return 429.
- Add integration tests for these limits (currently missing).

### 10. Security Hardening
- Ensure production deployments **cannot start** without `SECRET_KEY` and `JWT_SECRET_KEY` set. Test by unsetting these env vars and running the app; it should exit with clear error.
- Add **password complexity requirements** in registration (minimum length, mix of character types). Currently any password accepted.
- Implement **account lockout** after 5 failed login attempts to prevent brute force (currently unlimited attempts).

---

## Medium-Priority Improvements (Next Sprint)

### 11. Add NaN/Inf Protection in Model Inference
- The current checks only in final mix; also check model outputs:
  ```cpp
  if (std::isnan(wet_sample) || std::isinf(wet_sample)) wet_sample = 0.0f;
  ```
- Consider logging warning when NaNs detected to aid debugging.

### 12. Refactor STFTProcessor Frame Loop (Batching)
- The current implementation processes each frame in a loop, calling `fftwf_execute` each time. This is fine for small blocks, but could be optimized by batching multiple frames with a single FFT plan if the host provides large buffers. Evaluate if needed.

### 13. Replace C‑Style Memory Management with RAII
- Wrap FFTW buffers in `std::unique_ptr` with custom deleter:
  ```cpp
  using FFTWFBuffer = std::unique_ptr<float, void(*)(void*)>;
  FFTWFBuffer fftw_real_buffer_{fftwf_alloc(window_size_), &fftwf_free};
  ```
- This ensures exception safety and reduces manual `free` calls.

### 14. Add Fallback to Torch for Platforms Without FFTW
- In CMake, if `FFTW3` not found, fall back to the old Torch implementation and set a compile‑time define `USE_TORCH_FFT`.
- Update code with `#ifdef USE_TORCH_FFT` to choose implementation at compile time.
- Document this fallback path.

---

## Testing Checklist

Before marking this milestone complete:

- [ ] Both plugins compile without warnings on Linux (GCC 11+).
- [ ] All quick‑win fixes are verified (secrets, password check, S3 validation, concurrency limit, rate limiting).
- [ ] FFTW unit tests pass (round‑trip SNR > 80 dB).
- [ ] Latency benchmarks meet target (<0.5 ms for 256‑sample block).
- [ ] No heap allocations in `processBlock` (Valgrind massif shows zero heap growth during sustained audio).
- [ ] Audio quality validation shows <0.1 dB RMS difference.
- [ ] Documentation updated (BUILD_AND_TEST_GUIDE.md).
- [ ] Backend security tests pass (missing env vars cause startup failure).
- [ ] Integration test: user can submit job, worker processes, result downloads successfully.

---

## Rollback Plan

If critical issues are discovered:
1. Git branch `refactor/fftw-swap` contains all FFTW changes. Can be reverted if needed.
2. Previous version (Torch‑FFT) is preserved in commit history; rebuild old plugins if necessary.
3. Database migrations are **not** part of this refactor; rollback is safe.

---

## Ownership & Handoff

- **C++ audio code:** Responsible engineer should understand FFTW basic usage and plan lifecycle.
- **Backend API:** Ensure test coverage for new rate limiting and concurrency checks.
- **DevOps:** Add FFTW to Docker image (`Dockerfile`) and CI build scripts.

---

**Next:** Begin with **Build Verification** – install FFTW on a clean build machine and compile both plugins.
