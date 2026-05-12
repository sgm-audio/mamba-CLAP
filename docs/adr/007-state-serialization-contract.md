# ADR-007: Plugin State Serialization Contract

## Status
Accepted

## Context
Session save/restore must preserve plugin behavior while honoring RT ownership of live recurrent state.

## Decision
Serialized plugin state payload must include:
1. Plugin parameters.
2. Weight file path.
3. Base64-encoded snapshot of `h_state`.

### Serialization Rules
- Snapshot source is the audio-thread-owned state copied to secondary snapshot buffer (ADR-003 seqlock path).
- Main thread serializes only stable snapshot data.
- Deserialization occurs on main thread; reconstructed runtime context is published to audio thread at safe handoff point.

### Compatibility
- Serialized payload includes schema version and model config checksum.
- On mismatch, host restores parameters + weight path and resets `h_state` to zero with warning routed to NRT diagnostics.

### Security and Robustness
- Weight path canonicalization required before load.
- Base64 payload size limits enforced.
- Invalid decode must fail gracefully to reset-state behavior, never RT crash.

## Consequences
### Positive
- Reliable continuity across sessions and offline recalls.
- Explicit separation between live RT state and serialized snapshots.

### Negative
- Larger state payloads for high-dimension models.
- Versioning discipline required across exporter/runtime.

## Verification Requirements
- Round-trip tests for serialize/deserialize with deterministic state equivalence.
- Backward compatibility tests for older schema versions.

## Acceptance Criteria
- Saved state always includes params + weight path + base64 `h_state` snapshot.
- Restore path never mutates RT live state directly from main thread.
