# MambaFX — Production-Ready C++20 Plugin Architecture

**Plugin name**: MambaFX  
**Format**: CLAP ≥ 1.2 with optional ARA 2 (≥ 2.2) via clap-ara bridge  
**AI model**: Mamba-3 SSM — Tiny (20 MB), Base (80 MB), Large (150 MB) TorchScript  
**Audio task**: Context-aware adaptive audio mastering (reverb, compression, saturation, EQ)  
**Target DAWs**: Reaper, Bitwig Studio, Tracktion Waveform (all CLAP-native)  
**Language**: C++20, LibTorch (libtorch), clap-sdk ≥ 1.2, ARA2 SDK ≥ 2.2  

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
├── dsp/                         ← Real-time-safe DSP; zero alloc after activate()
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

### 3.3 Real-Time Safety Guarantees (Audio Thread)

The audio thread (`PluginCore::process()`) is certified **real-time safe**:

| Prohibited operation          | Enforcement mechanism                                    |
|-------------------------------|----------------------------------------------------------|
| `malloc` / `new`              | All buffers pre-allocated in `activate()`; ASAN in tests |
| `std::mutex::lock()`          | No mutex in audio path; only SPSC queues and atomics     |
| LibTorch forward pass         | Inference runs exclusively on the worker thread          |
| File I/O                      | No disk access in `process()`                            |
| `std::this_thread::sleep_for` | Never called in process path                             |
| Syscall `mmap` / `mprotect`   | All memory mapped at activation time                     |

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

## 5. Memory Management — 150 MB Mamba Model

### 5.1 Loading Strategy

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
# Minimum production build:
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

## 8. Key Design Decisions & Trade-offs

| Decision                                  | Why                                               | Trade-off                                      |
|-------------------------------------------|---------------------------------------------------|------------------------------------------------|
| SPSC queues instead of mutexes on RT path | Wait-free; no priority inversion                  | Fixed capacity; overflow silently drops events |
| Mel-spectrogram → Mamba (not raw waveform)| Reduces sequence length 256×; proven audio ML rep  | Phase discarded; STFT/ISTFT overhead ≈2 ms    |
| WeightStore process-global singleton      | Multiple DAW instances share memory-mapped weights | Requires version/path consistency across slots |
| MambaSession::step() catches all exceptions | Torch may throw on NaN state; RT thread must never crash | Errors silently reset state; no RT-safe logging |
| ARA analysis on InferenceQueue pool       | Offline; no real-time budget; can use full Torch  | Pool workers and RT worker share CPU core count |
| Phase vocoder (not Griffin-Lim)           | Single-pass; Griffin-Lim needs 30+ iterations     | Phase distortion on transients; acceptable for mastering |
| StateSerializer version migration table  | Future-proof against parameter schema changes     | Old presets need manual testing on each version |
