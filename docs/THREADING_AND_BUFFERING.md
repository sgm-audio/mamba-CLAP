# Threading and Buffering Strategy for Mamba-SSM

Based on the highly optimized C++ architecture utilizing AVX2 SIMD intrinsics, our Mamba-SSM inference requires ~133 nanoseconds per sample. This allows us to run inference **directly on the audio thread** without violating the 5.8ms budget (for 256 samples @ 44.1kHz). 

## 1. Component & Threading Diagram

Because inference executes on the audio thread, there is no need for complex audio lock-free ring buffers or priority inversion handling between the DSP and an inference thread. Threading is strictly segregated between UI/Host automation (Main Thread) and DSP (Real-Time Thread).

```mermaid
graph TD
    subgraph Host Main Thread / UI
        UI[Plugin GUI]
        Autom[Host Automation]
        Load[MambaWeightLoader]
    end

    subgraph Lock-Free Boundary
        Params[std::atomic Param Queue]
        Weights[(Static Read-Only Weights)]
    end

    subgraph Real-Time Audio Thread
        Process[clap_process_t]
        
        subgraph MambaModel
            StateH[(state_h_ persistent)]
            ConvH[(conv_buffer_ persistent)]
            SIMD[AVX2 Vectorized Math]
        end
    end

    UI --> Params
    Autom --> Params
    Load --> Weights
    
    Params -.->|Read| Process
    Weights -.->|Read| SIMD
    
    Process --> SIMD
    SIMD <--> StateH
    SIMD <--> ConvH
    SIMD --> Process
```

## 2. Exact Buffering Scheme & Latency

Because inference is directly on the audio thread and processes strictly sample-by-sample:

*   **Inference Location:** Real-Time Audio Thread.
*   **Internal Chunk Size ($C$):** $C = 1$ sample. The `MambaLayer::processSample()` natively accepts 1 sample at a time.
*   **Ring Buffer Size:** `0` (No audio ring buffers required). The only internal ring buffer is the exact causal history needed for Mamba's `conv1d` layer (`[d_inner * d_conv]`), which is maintained per-layer.
*   **Variable Host Block Size:** Supported natively. A block size of 32, 128, or 512 simply loops the sample processor $N$ times. No fractional chunk tracking is needed.
*   **Worst-Case Algorithmic Latency:** **$0$ samples**.

### Transport & Bypass Behavior

*   **Transport Seek / Playback Start:** 
    The plugin intercepts `clap_event_transport` events from `process->in_events`. If the transport jumps (e.g., `flags & CLAP_TRANSPORT_IS_PLAYING` transitions from false to true, or the playhead position discontinuously jumps), `MambaModel::resetState()` is called to flush `state_h_` and `conv_buffer_` to zero, preventing historical state from bleeding into a new timeline position.
*   **Bypass Behavior (Freeze vs. Track):**
    If the plugin is hard-bypassed via the host, the audio callback routes dry signal directly to output. The internal Mamba state **freezes** (does not update) to save CPU cycles. When un-bypassed, a `resetState()` is automatically triggered so that the model re-initializes its context cleanly rather than continuing from a temporally disconnected state.

## 3. Realtime Safety Checklist

The plugin implements the following rigid checks aligned with the CLAP lifecycle:

| Callback | Action / Validation Checklist |
| :--- | :--- |
| `init()` | ✅ Run `MambaWeightLoader` (File I/O).<br>✅ Initialize parameter metadata. |
| `activate()` | ✅ Pre-allocate all `std::vector` state buffers for the given sample rate.<br>✅ Size validation (ensure exact capacity fits `d_inner * d_state`).<br>✅ Lock memory if supported by OS to prevent paging. |
| `process()` | ✅ Assert no locks are held.<br>✅ Assert no `malloc`/`free`/`new`/`delete` calls.<br>✅ Drain `in_events` queue into atomics before audio loop.<br>✅ Interpolate parameters per-sample to avoid zipper noise.<br>✅ Enforce AVX flush-to-zero bounds for subnormal floats. |
| `deactivate()` | ✅ Reset `state_h_` and `conv_buffer_` to zero.<br>✅ Free pre-allocated vectors (safe here, NRT thread). |
| `flush()` | ✅ Drain parameter queue when audio engine is stopped. |
