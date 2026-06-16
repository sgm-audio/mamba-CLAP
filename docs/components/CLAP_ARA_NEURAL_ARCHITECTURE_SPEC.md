# CLAP/ARA Neural Audio Architecture Spec

## Scope
- Standard targets: CLAP 1.1+, ARA 2.0, C++20, CMake.
- Hosts: Reaper, Bitwig, Studio One, Logic via wrapper where required, Cubase.
- Platforms: macOS Universal, Windows 10+, Linux (optional LV2 wrapper target).

## Feature Decomposition by Domain

| Feature | Domain | Thread Owner | Notes |
| :--- | :--- | :--- | :--- |
| `process()` audio entry and block iteration | Host API | Audio Thread | Delegates to SSM runtime only via lock-free inputs. |
| Recurrent selective-scan step and `h_state` mutation | SSM Core | Audio Thread | Sole mutable owner of live hidden state. |
| Mode A tiny micro-block scheduling | SSM Core | Audio Thread | `4-16` sample micro-block, no lookahead required. |
| Mode B block scheduling and overlap-add policy | SSM Core | Audio Thread | `64-512` chunks, carries state over boundaries. |
| CLAP parameter ingest and flush | Host API | Main Thread | Publishes events to lock-free parameter channel. |
| Host automation visualization and GUI controls | UI/State | Main Thread | Never mutates live RT state directly. |
| ARA document/context extraction | Host API | ARA Thread | Produces musical context vectors. |
| ARA conditioning encode to `cond[COND_DIM]` | UI/State | ARA Thread | Encodes context to POD frame for RT delivery. |
| 1x1 projection and SSM biasing of `delta` and `B` | SSM Core | Audio Thread | Uses immutable preloaded projection weights. |
| Weight file discovery, canonicalization, mmap | ML Pipeline | Main Thread | Read-only mapped storage only. |
| Generated `config.h` ingestion/validation | ML Pipeline | Main Thread | Compile-time and startup guards. |
| Session save/load (params + path + `h_state`) | UI/State | Main Thread | Uses seqlock snapshots from audio thread. |
| Snapshot publication (`h_state`) | SSM Core | Audio Thread | Writes secondary buffer only. |
| Snapshot read and base64 encode | UI/State | Main Thread | Retries on seqlock instability. |

## Triple-Thread Boundary Rules
1. Audio Thread
- Owns `process()` and live `h_state`.
- Reads atomics/queues only.
- Hard constraints: zero alloc/free, zero locks, zero file I/O, zero logging.

2. Main Thread
- Owns UI, CLAP flush, serialization, weight load, runtime context prepare/swap.

3. ARA Thread
- Owns `DocumentController` updates, context extraction, offline render orchestration.

## Lock-Free Communication Summary
- Parameters: SPSC queue or atomic float array.
- ARA conditioning: atomic double-buffered `ConditioningFrame` aligned to 128-bit minimum.
- State snapshots: audio writes secondary state with seqlock, main retries until stable.

## Memory Architecture Summary
- Pre-allocate all inference scratch and state in `activate()` using known max block size.
- Weight storage from read-only mmap, cache-aligned.
- Model dimensions fixed by generated `config.h`.

## Model Scalability Contract
Swapping a 1M model for a 50M model is a configuration and artifact update only:
1. Replace generated `config.h`.
2. Replace weight file.

No subsystem API rewrite is required if contracts remain unchanged.
