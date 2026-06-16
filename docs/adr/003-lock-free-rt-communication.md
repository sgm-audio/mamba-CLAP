# ADR-003: Lock-Free Cross-Thread Communication Contracts

## Status
Accepted

## Context
The plugin must exchange parameters, ARA context, and state snapshots across `Main`, `ARA`, and `Audio` threads without violating RT constraints.

## Decision
Adopt lock-free communication contracts per channel.

### 1. Parameter Changes (`Main -> Audio`)
Use one of two approved paths:
1. `SPSC ring buffer` carrying time-stamped parameter events.
2. `Atomic float array` for direct latest-value access.

Rules:
- Producer: main thread only.
- Consumer: audio thread only.
- Queue capacity fixed at `activate()`.
- Overflow policy must be explicit (`drop oldest` or `coalesce by param id`).

### 2. ARA Conditioning (`ARA -> Audio`)
Use an atomic double-buffered `ConditioningFrame` contract:
- Two aligned frame slots (`128-bit` aligned minimum).
- Producer writes inactive slot fully, then atomically flips active index/version.
- Audio thread reads active slot by acquire semantics.

Rules:
- No partial visibility allowed.
- Frame payload is POD/trivially copyable.

### 3. State Snapshots (`Audio -> Main`)
Use secondary state buffer with seqlock:
- Audio thread copies `h_state` into snapshot buffer and increments sequence.
- Main thread retries read if sequence changed or odd while reading.

Rules:
- Audio thread is exclusive writer.
- Main thread is read-only observer.
- Snapshot cadence bounded (for example every N blocks) to control CPU overhead.

## Consequences
### Positive
- Deterministic communication without locks/mutexes.
- No ownership ambiguity around live vs snapshot state.

### Negative
- More explicit versioning and overflow handling logic.
- Snapshot frequency must be tuned to avoid extra RT cost.

## Verification Requirements
- Unit tests must validate queue overflow policy, ARA frame atomic swap correctness, and seqlock retry behavior under synthetic contention.
- TSAN-enabled non-RT harnesses must report no data races on message structures.

## Acceptance Criteria
- Parameter and ARA updates can be delivered while audio is running with zero mutex usage in RT path.
- Main thread can read consistent `h_state` snapshots without pausing audio thread.
