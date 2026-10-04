# MambaFX — proposed C++20 plug-in architecture

> **Status (2026-10-04): design proposal / incomplete implementation.** This document is not a release specification or evidence that the listed modules, Mamba-3 model, CLAP/ARA support, or DAW compatibility exist. The separate `plugin/` tree has missing build sources and has not passed a complete build, host, audio-quality, or real-time audit. See [`../docs/PROJECT_STATUS.md`](../docs/PROJECT_STATUS.md).

**Proposed format**: CLAP, with optional ARA 2 exploration (neither release-validated)
**Candidate model**: sequence model not yet selected or integrated; Mamba-3 is research input only (no validated plug-in checkpoint or TorchScript path)
**Proposed audio task**: context-informed control of conventional DSP effects; musical benefit and quality not evaluated
**Target DAWs**: targets for future compatibility testing, not verified hosts
**Proposed language/runtime**: C++20; required SDK/dependency versions need validation

---

## 1. Directory & Module Layout

```
plugin/
├── CMakeLists.txt               ← Build entry; options: CUDA, ARA, UI, tests
│
├── core/                        ← Platform-independent business logic
│   ├── PluginCore.hpp/cpp       ← Ownership root; created once per plugin instance
│   ├── ParameterBank.hpp/cpp    ← Atomic parameter store + CLAP info descriptors
│   ├── TransportState.hpp       ← Atomic mirror of host transport (written audio-RT)
│   └── EventQueue.hpp           ← Lock-free SPSC queues (AudioEvent, InferenceResult)
│
├── clap/                        ← Thin C-to-C++ adapter (no business logic here)
│   ├── ClapPlugin.hpp/cpp       ← clap_plugin_t vtable; owns PluginCore
│   ├── ClapExtensions.hpp/cpp   ← params, state, latency, tail, audio-ports vtables
│   └── ClapFactory.hpp/cpp      ← clap_plugin_factory_t + clap_entry export symbol
│
├── ara/                         ← ARA 2 document model (compiled only if MAMBA_ENABLE_ARA)
│   ├── ARADocumentController.hpp/cpp  ← Document lifecycle, region callbacks, analysis dispatch
│   ├── ARAPlaybackRenderer.hpp/cpp    ← Offline audio rendering per ARA region
│   ├── ARAEditorRenderer.hpp/cpp      ← Real-time editor preview (waveform display)
│   ├── ARAEditorView.hpp/cpp          ← ARA editor view host integration
│   └── ARARegionAnalyzer.hpp/cpp      ← Stateless: reads ARA samples → AudioFeatures
│
├── model/                       ← ML inference; never includes clap/ or ara/ headers
│   ├── MambaInferenceEngine.hpp/cpp   ← Worker thread; ring-buffer → session → results
│   ├── MambaSession.hpp/cpp           ← Per-context Mamba recurrent state (step / segment)
│   ├── ModelLoader.hpp/cpp            ← Async load + WeightStore (process-global cache)
│   └── InferenceQueue.hpp/cpp         ← Thread pool for ARA offline analysis jobs
│
├── dsp/                         ← Proposed callback DSP components; safety not audited
│   ├── AudioRingBuffer.hpp/cpp        ← Lock-free SPSC multi-channel ring buffer
│   ├── STFTEngine.hpp/cpp             ← STFT analysis + phase-vocoder ISTFT (kissFFT)
│   ├── OverlapAddSynthesizer.hpp/cpp  ← OLA synthesis → host output buffer
│   └── EffectApplicator.hpp/cpp       ← Decodes model params → DSP coefficients + apply
│
└── state/                       ← Serialisation bridge
    ├── StateSerializer.hpp/cpp   ← CLAP state blob ↔ ParameterBank; migration table
    └── PresetBank.hpp/cpp        ← Factory presets + user preset save/load
```

---

## 2. Class Hierarchy & Ownership

```
clap_entry  (extern "C", static storage)
    └── clap_plugin_factory_t  kPluginFactory  (static)
            └── ClapPlugin::factory_create_plugin()
                    └─► new ClapPlugin                          [heap, unique per instance]
                            │
                            ├── clap_plugin_t  plugin_          [value member, C handle]
                            │
                            └── unique_ptr<PluginCore>           [owns everything below]
                                    │
                                    ├── unique_ptr<ParameterBank>        [atomic param store]
                                    ├── unique_ptr<TransportState>       [atomic transport]
                                    ├── unique_ptr<AudioCommandQueue>    [UI→Audio SPSC]
                                    ├── unique_ptr<InferenceResultQueue> [Inference→Audio SPSC]
                                    │
                                    ├── unique_ptr<AudioRingBuffer>      [pre-alloc, activate()]
                                    ├── unique_ptr<STFTEngine>           [pre-alloc, activate()]
                                    ├── unique_ptr<OverlapAddSynthesizer>[pre-alloc, activate()]
                                    ├── unique_ptr<EffectApplicator>     [pre-alloc, activate()]
                                    │
                                    ├── unique_ptr<MambaInferenceEngine>
                                    │       ├── unique_ptr<ModelLoader>
                                    │       │       └── WeightStore::instance()   [process singleton]
                                    │       │               └── shared_ptr<torch::jit::Module>
                                    │       ├── unique_ptr<MambaSession>          [current RT session]
                                    │       ├── unique_ptr<InferenceQueue>        [ARA job pool]
                                    │       └── std::thread  worker_              [inference loop]
                                    │
                                    ├── unique_ptr<StateSerializer>
                                    │
                                    └── unique_ptr<ARADocumentController>  [#ifdef MAMBA_ENABLE_ARA]
                                            ├── ARARegionAnalyzer          [value member]
                                            └── InferenceQueue&            [non-owning ref to engine's pool]

Lifetime invariants:
  • PluginCore outlives all sub-objects (destroyed last in ~PluginCore).
  • MambaInferenceEngine::shutdown() joins worker thread BEFORE DSP objects destroyed.
  • WeightStore singleton holds weak_ptr; module dropped when last Session releases it.
  • ARAPlaybackRenderer owns its own MambaSession (cloned from engine's active session).
  • All raw pointers in C vtables are non-owning; lifetimes guaranteed by ClapPlugin scope.
```

---

## 3. Thread Safety Strategy

### 3.1 Thread Map

```
┌───────────────────────────────────────────────────────────────────────────────┐
│  THREAD             │  PRIORITY     │  WHAT IT DOES                          │
├─────────────────────┼───────────────┼────────────────────────────────────────┤
│  Main / UI          │  Normal       │  CLAP init/destroy, params_flush,      │
│                     │               │  ARA document callbacks, preset UI      │
├─────────────────────┼───────────────┼────────────────────────────────────────┤
│  Audio RT           │  Real-time    │  process(): read events, run DSP,      │
│                     │  (SCHED_FIFO  │  push ring buffer, read results        │
│                     │   on Linux)   │                                         │
├─────────────────────┼───────────────┼────────────────────────────────────────┤
│  Inference Worker   │  Above-normal │  pop ring → STFTEngine → MambaSession  │
│                     │               │  → push InferenceResult                │
├─────────────────────┼───────────────┼────────────────────────────────────────┤
│  InferenceQueue[0]  │  Normal       │  ARA region analysis job 0             │
│  InferenceQueue[1]  │  Normal       │  ARA region analysis job 1             │
└─────────────────────┴───────────────┴────────────────────────────────────────┘
```

### 3.2 Cross-thread Communication

```
Main/UI thread                    Audio RT thread
──────────────────────────────    ─────────────────────────────────────────────
ParameterBank::set()              ParameterBank::get()          ← std::atomic<float>
                                                                  (relaxed ordering;
                                                                   exact-sample accuracy
                                                                   via input event queue)

AudioCommandQueue::push()    →    AudioCommandQueue::pop()       ← SPSCQueue<AudioEvent,256>
                                                                   lock-free, wait-free

InferenceResultQueue::push() ←    InferenceResultQueue::pop()   ← SPSCQueue<InferenceResult,64>
  (inference worker writes)          (audio thread reads)          lock-free, wait-free

TransportState::update()     ←    (written by audio thread       ← std::atomic<> fields
  seek_generation increment          only during process())         (release/acquire pairs)

                                  AudioRingBuffer::push()       → AudioRingBuffer::pop()
                                    (audio → ring)                  (inference worker)
                                    lock-free                        lock-free

WeightStore mutex                                                 ← Only held by loader thread;
  (model load only)                                                 audio thread never touches it

InferenceQueue mutex                                             ← Only held by pool threads;
  (ARA jobs only)                                                  audio thread never touches it
```

### 3.3 Real-time safety design goals (not audited)

The following are proposed constraints, not guarantees or verified implementation facts. The architecture is incomplete; no complete allocation, blocking, exception, worst-case timing, or host-callback audit has been run.

| Prohibited operation          | Proposed approach                                      | Evidence status |
|-------------------------------|--------------------------------------------------------|-----------------|
| `malloc` / `new`              | Pre-allocate callback buffers during activation       | Not verified end to end |
| `std::mutex::lock()`          | Keep blocking synchronization off the audio path      | Not verified end to end |
| LibTorch forward pass         | Run inference off the audio callback                  | Not wired/verified end to end |
| File I/O                      | Keep disk access outside `process()`                  | Not verified end to end |
| `std::this_thread::sleep_for` | Avoid waits in callback code                           | Not audited |
| Syscall `mmap` / `mprotect`   | Load/map model data before processing                 | Not verified |

---

## 4. State Serialisation Plan

### 4.1 Scope Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  PROJECT FILE (.rpp / .bwproject)                                           │
│                                                                             │
│  ┌───────────────────────────────┐  ┌────────────────────────────────────┐ │
│  │  CLAP PLUGIN STATE BLOB       │  │  ARA DOCUMENT ARCHIVE              │ │
│  │  (per FX-chain slot)          │  │  (per project, per ARA doc)        │ │
│  │                               │  │                                    │ │
│  │  [4B magic "MBFX"]            │  │  Per-audio-source:                 │ │
│  │  [4B version]                 │  │    persistent_id                   │ │
│  │  [8×4B param floats]          │  │    SourceAnalysis { rms, flux, CF }│ │
│  │  [4B path_len]                │  │                                    │ │
│  │  [UTF-8 model_root]           │  │  Per-arrangement:                  │ │
│  │                               │  │    last_analysis_json (string)     │ │
│  └───────────────────────────────┘  └────────────────────────────────────┘ │
│          │                                      │                           │
│          ▼                                      ▼                           │
│  StateSerializer::load()            ARADocumentController::                 │
│  → ParameterBank::restore()           doRestoreObjectsFromArchive()         │
│  → AudioCommandQueue::push(          → sources_ map rebuilt                 │
│      ParamChanged × N)               → schedule_analysis() skipped if       │
│  → notify model reload if variant      sources_ has all IDs already         │
│      or device changed                                                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Parameter ↔ ARA Document Sync Rules

| CLAP Param          | Triggers ARA re-analysis? | Rationale                                    |
|---------------------|---------------------------|----------------------------------------------|
| `ModelVariant`      | Yes                       | Different model → new analysis quality       |
| `ContextLength`     | Yes                       | Analysis window changes                      |
| `EffectType`        | No                        | Effect choice is real-time; ARA is source-level |
| `WetMix`            | No                        | Pure RT parameter                            |
| `InputGain`         | No                        | Gain staging only                            |
| `OutputGain`        | No                        | Gain staging only                            |
| `InferenceDevice`   | Yes (partial)             | Device change requires session re-init       |
| `Bypass`            | No                        | Pass-through flag only                       |

---

## 5. Proposed model memory and loading strategy

No checkpoint or model size is selected; memory estimates are undetermined. The flow below is a design sketch, not verified implementation. The worker source is incomplete, `MambaSession::step()` builds temporary TorchScript argument vectors, and mmap/CoW savings, loading time, allocation behavior, and callback handoff have not been measured.

### 5.1 Proposed loading sequence

```
Startup
  │
  ├── ModelLoader::load_async()        [background thread; never blocks audio]
  │       │
  │       ├── WeightStore::get_or_load()
  │       │       ├── Check weak_ptr cache (no alloc if already loaded by another instance)
  │       │       └── torch::jit::load(path)       ← mmap-backed deserialization
  │       │               • OS lazy-loads pages on first access
  │       │               • Weights pinned read-only; CoW shared across instances
  │       │
  │       ├── module->to(device)                   ← CPU: no-op; CUDA: async H→D copy
  │       └── MambaInferenceEngine::on_model_ready()
  │               ├── new MambaSession(module, n_mels, device)
  │               │       ├── Pre-allocate input_scratch_ [1×80 float]
  │               │       └── Pre-allocate state_[L] [1×d_model×d_state × L layers]
  │               ├── active_session_.store(new_session, seq_cst)
  │               └── AudioCommandQueue::push(ModelReloaded)  [lock-free]
  │
  └── host->request_callback()         ← host schedules on_main_thread() for UI refresh
```

### 5.2 Memory Budget (Base model, CPU)

```
Component                              Approx. Size
──────────────────────────────────────────────────────
TorchScript model weights (mmap)        80 MB   ← shared across instances via OS CoW
MambaSession recurrent state           ~2 MB    ← per instance; L=12 layers, d_state=64
STFT/OLA buffers (pre-allocated)       ~4 MB    ← n_fft=1024, 32-frame ring
AudioRingBuffer (×2 for stereo)        ~2 MB    ← capacity=8×max_block, max_block=4096
InferenceResult queue                  <1 KB    ← 64 × 16 bytes
──────────────────────────────────────────────────────
Total per instance (excl. shared)     ~8 MB
Total shared (first instance loads)   ~80 MB
Total for N instances                  80 + 8N MB
```

### 5.3 Streaming / Caching

```
Audio RT Thread                  Inference Worker Thread
──────────────────               ──────────────────────────────────────────
process() called every           Blocked on condition_variable or ring-buffer poll
  ~5.8 ms (256 samples           (uses nanosleep with 1 ms granularity to avoid
  @ 44.1 kHz)                     spinning at full CPU)
     │                                 │
     │ push(raw_samples)               │ pop(raw_samples)
     │ → AudioRingBuffer               │ → STFTEngine::process()
     │   (capacity = 2048×8           │      apply Hann window
     │    frames = 18 ms headroom)    │      FFT (kissFFT, stack-alloc)
     │                                │      mag_to_mel()
     │                                │      MambaSession::step()   ← O(d_state²) per step
     │                                │      EffectApplicator::apply_to_mel()
     │ pop(result)  ◄─────────────────│ push(InferenceResult)
     │ → mix wet/dry                  │
     │ → OverlapAddSynthesizer         │
     │ → host output buffer           │
```

### 5.4 Hot-Swap (Zero Glitch Model Reload)

```
Old session active → user changes ModelVariant → StateSerializer stores new variant
    │
    ├── MambaInferenceEngine::load_async(new_variant)   [background thread]
    │       • WeightStore loads new weights
    │       • new MambaSession allocated with new module
    │
    ├── active_session_.store(new_ptr, seq_cst)         [atomic swap]
    │
    ├── AudioCommandQueue::push(ModelReloaded)           [lock-free signal]
    │
    ├── Old MambaSession kept alive in pending_session_  [no use-after-free]
    │       until inference worker thread has passed the swap point
    │
    └── pending_session_.reset()                         [old memory freed off RT thread]
```

---

## 6. ARA 2 Integration Detail

### 6.1 Role Mapping

| ARA Role                | MambaFX class                  | Purpose                              |
|-------------------------|-------------------------------|--------------------------------------|
| DocumentController      | `ARADocumentController`       | Arrangement analysis; archive        |
| PlaybackRenderer        | `ARAPlaybackRenderer`         | Offline Mamba rendering per region   |
| EditorRenderer          | `ARAEditorRenderer`           | Real-time preview in editor          |
| EditorView              | `ARAEditorView`               | Host-provided editor window          |

### 6.2 ARA Offline Processing Flow

```
Host calls willBeginEditing()
  → editsInProgress = true; suppress analysis triggers

Host calls didEndEditing()
  → editsInProgress = false
  → edit_generation_++
  → InferenceQueue::submit(run_arrangement_analysis)  [on pool thread]
        │
        ├── collect_regions()      → vector<RegionMeta>
        ├── ARARegionAnalyzer::compute_features() per source
        │       → ARA sample access (host provides float* directly, no decode)
        │       → RMS, spectral flux, crest factor, ZCR
        ├── Build JSON prompt for Mamba inference engine
        ├── MambaSession::process_segment()    ← full offline, no RT budget
        └── on_analysis_ready_(json_result)    ← callback to UI / ARADocumentController
```

---

## 7. Build Configuration Summary

```cmake
# Proposed dependency/configuration example (not verified to build):
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCLAP_PATH=/opt/clap-sdk \
      -DTORCH_INSTALL_PATH=/opt/libtorch \
      -DARA_SDK_PATH=/opt/ara2-sdk \
      -DCLAP_ARA_PATH=/opt/clap-ara \
      -DMAMBA_ENABLE_ARA=ON \
      -DMAMBA_ENABLE_CUDA=OFF \
      -DMAMBA_ENABLE_UI=ON

# Test build with sanitizers:
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug \
      -DMAMBA_BUILD_TESTS=ON -DMAMBA_SANITIZE=ON ...
```

---

## 8. Proposed Design Decisions & Trade-offs

These are hypotheses for a future implementation, not verified product results.

| Candidate decision | Rationale to test | Open risk / evidence needed |
|---|---|---|
| SPSC queues instead of locks on the audio path | May avoid blocking when correctly implemented | Audit memory ordering, overflow behavior, ownership, and worst-case callback time |
| Mel-spectrogram input to a candidate sequence model | May shorten the model sequence compared with raw waveform input | Measure task quality, phase/reconstruction artifacts, and transform overhead |
| Shared model-weight storage across instances | May reduce duplicate memory use | Validate lifetime, synchronization, path/version consistency, and actual memory savings |
| Inference off the audio callback | Intended to isolate unbounded model work | End-to-end worker/result handoff is incomplete; measure freshness, underruns, and CPU contention |
| Candidate phase-vocoder/OLA reconstruction | May provide a practical reconstruction path | Compare alternatives and use listening tests for transient/artifact acceptability |
| Versioned state migration | Intended to support preset compatibility | Requires a real serializer, fixtures, and upgrade tests |
