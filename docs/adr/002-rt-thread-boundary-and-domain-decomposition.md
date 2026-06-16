# ADR-002: Real-Time Thread Boundary and Domain Decomposition

## Status
Accepted

## Context
This plugin suite must run neural audio processing under CLAP 1.1+ and ARA 2.0 while preserving hard real-time guarantees in `process()`. The implementation spans CLAP host integration, Mamba-SSM inference, offline ARA analysis, and UI/session state.

Without explicit domain boundaries and thread ownership, teams will introduce mutexes, allocations, or cross-thread state hazards inside the audio callback.

## Decision
We define four architectural domains and a strict triple-thread boundary.

### Domain Map
1. `SSM Core`
- Owns recurrent hidden state (`h_state`) math and selective scan stepping.
- Exposes sample/micro-block/block stepping APIs.
- Contains no host-specific logic.

2. `Host API`
- Owns CLAP plugin lifecycle, CLAP params/events, and ARA bridge entry points.
- Translates host events into lock-free data messages for RT use.

3. `ML Pipeline`
- Owns weight file loading, model descriptor parsing, mmap setup, runtime validation, and compile-time generated config contract.
- Performs all non-RT preparation in `init()` and `activate()`.

4. `UI/State`
- Owns GUI, automation presentation, state serialization/deserialization, preset/session logic, and parameter publishing.
- Never directly mutates RT-owned SSM state.

### Triple-Thread Boundary
1. `Audio Thread` (hard RT)
- Owns `process()` execution.
- Sole mutable owner of live `h_state`.
- Reads atomics and lock-free message queues only.
- Forbidden: malloc/free, locks, file I/O, logging, blocking syscalls, exceptions crossing callback.

2. `Main Thread` (NRT)
- Owns GUI, CLAP param flush, plugin state save/load, model weight loading, and rebuild of prepared runtime contexts.
- Publishes updates to RT via atomics/ring buffers only.

3. `ARA Thread` (NRT/offline)
- Owns `DocumentController` updates and musical context extraction.
- Produces conditioning frames and offline render requests.
- Communicates to RT through double-buffered atomics only.

## Consequences
### Positive
- Real-time safety obligations are localized and testable.
- Teams can implement subsystems independently with deterministic ownership.
- Host and ARA integration no longer leak into SSM math layer.

### Negative
- Additional interface scaffolding and message contracts are required.
- Some convenience APIs are disallowed when they violate thread law.

## Verification Requirements
- Static analysis and review checklists must tag every public method with its owning domain and allowed calling thread.
- Runtime debug assertions must fail if forbidden RT operations are detected in debug instrumentation builds.

## Acceptance Criteria
- Every feature is assignable to one of: `SSM Core`, `Host API`, `ML Pipeline`, `UI/State`.
- A junior engineer can identify exactly which thread may call and mutate each subsystem entry point.
