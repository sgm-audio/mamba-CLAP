# Sanitize manifest: mamba-CLAP (2026-10-31)

Branch `chore/sanitize`, cut from `master` @ `f5a0158`. 4 local commits, including this file. Nothing pushed. There was no open PR to fold in.

## Removed
| Path | Files | Why |
|---|---|---|
| `MambaDAW/build/` | 39 | Build output: an 8.2 MiB ELF `MambaClapPlugin.clap`, CMakeCache with local paths, and a `_deps/clap-src` gitlink. |
| `MambaDAW/bench_mamba`, `MambaDAW/test_mamba_layer`, `MambaDAW/tests/param_gate_test`, `MambaDAW/tools/benchmark` | 4 | ELF binaries committed next to their sources. |
| `MambaDAW/python_training/resources/mamba_model.pt`, `mamba_model_final.pt` | 2 | No provenance. `train_mamba.py` writes them. |
| `mamba-compressor/` | 36 | An older, diverged copy of sgm-audio/mamba-compressor. The README links the standalone repo. |
| `.vscode/settings.json` | 1 | Local paths. |
| `AGENTS.md`, `GEMINI.md` | 2 | Agent instructions. |
| `emotion-aware-dsp` gitlink | 1 | Its URL (SGM-Studios/emotion-aware-dsp) returns 404 publicly, so a clone can't fetch it. |

`clap-plugin/clap-sdk` was a gitlink with no `.gitmodules` entry. It's now a real submodule of free-audio/clap at the same commit (`29ffcc27`, tag 1.2.7). `.gitmodules` now lists only that submodule.

## Moved
- To `docs/history/`, kept for reference: `FINAL_IMPLEMENTATION_SUMMARY.md`, `NEXT_STEPS.md`, `docs/IMPLEMENTATION_COMPLETE.md`, `docs/IMPLEMENTATION_SUMMARY.md` and `MambaDAW/IMPLEMENTATION_SUMMARY.md` (renamed `MambaDAW_IMPLEMENTATION_SUMMARY.md`).
- `PRODUCTION_READINESS.md` → `docs/`.
- `PRD.md`, `SYSTEM_DESIGN.md`, `TEST_PLAN.md` → `docs/design/`.
- `api-spec.yaml` → `docs/api/`.
- The moves broke 17 links; all are fixed (0 broken). The broken links to `mamba_ssm/` from `docs/api/index.md` were also fixed.

## Added
`.gitignore` (build/, target/, `*.clap`, binaries, `*.pt`/onnx, agent files) and `scripts/ci.sh`. LICENSE was already MIT ("SGM Studios"), so it's unchanged.

## Code fixes (tested)
- `RingBuffer::new(n)` now holds n items, using n + 1 slots with one kept empty. Before, `new(2)` accepted only one push, and `test_ring_buffer_full` and `test_ring_buffer_close` failed. New test: capacity holds after the indices wrap.
- `CrossfadeBuffer::reset()` restores the `new(length)` state, which `test_crossfade_reset` expected.
- `test_crossfade_apply` asserted exact values like 0.33 and 1.33 that no linear fade produces. It now checks the implemented ramp (wet gain p/length at position p) with a 1e-6 tolerance, plus pure wet after the fade. New test: dry and wet gains sum to 1 at every step. This is a change to an existing test's expected values; they were wrong, not loosened.
- `mamba_layer.cpp:19`: cast `RAND_MAX` to float. Clang's `-Wimplicit-const-int-float-conversion` broke `-Werror`.
- `docs/data-model.md`: the example `accessToken` is replaced by `<jwt>`. It was a 39-character placeholder that gitleaks flagged.

## Doc claims changed
- The README status date is now 2026-10-31.
- Added a before/after test table with machine and toolchain, a map of where the maintained code lives (EmT, clara, mamba3, mamba-compressor), and a layout section.
- The mamba-compressor row says it moved.
- The research/prototype framing is unchanged; it was already accurate.

## Tests, before and after (shared 8-vCPU Intel Xeon VM, Linux, zig clang 22.1.8, rustc/cargo 1.99.1)
| Check | Before (`master` @ f5a0158) | After (fresh clone, `scripts/ci.sh`) |
|---|---|---|
| `cargo test` | 22 passed, 4 failed | 28 passed, 0 failed |
| C++ layer test, `-O2 -Wall -Wextra -Wpedantic -Werror` | compile error (implicit int→float); 7/7 without `-Werror` | 7/7 |
| C++ parameter-gate test, `-O2 -Werror` | not run | passes |
| Plugin builds (MambaDAW, `plugin/`, `clap-plugin/`) | not built | not built (blockers in `docs/PROJECT_STATUS.md`) |
| gitleaks dir | 1 (placeholder JWT) | 0 |
- cargo builds with 8 unused-import/variable warnings. `clippy -D warnings` isn't in `ci.sh`.
- `scripts/ci.sh` exits non-zero on any failure (`set -euo pipefail`).
- Logs: `_logs/mclap-before.log`, `mclap-after.log`.

## Needs Scott
- **emotion-aware-dsp:** is it private or deleted? The gitlink was removed either way.
- **The vendored `mamba/` tree:** keep it (Apache-2.0, LICENSE present) or replace it with a link.
- **GitHub description and topics.** Proposed: "Research archive: Mamba/SSM experiments for audio, early CLAP prototypes and design notes. Not production code." Topics: mamba, ssm, audio-ml, clap, research.
- **History rewrite approval** (below).

## History rewrite (prepared, not run on the real repo)

Nothing has been rewritten or pushed. The commands below were tried on a throwaway `--mirror` clone in `/tmp` on 2026-10-31. They exited 0, and afterwards `git log --all -p` and the commit messages had 0 matches for the local-path patterns. Pack size went from 122.57 MiB to 2.63 MiB. That scratch clone was deleted.

The secret scans (gitleaks `git` and `dir`, and trufflehog, over all history) found one hit: a 39-character `accessToken` example in `data-model.md`, which is a placeholder, not a real JWT. So this rewrite is about local paths and large or unneeded files, not live keys. Scott says any exposed keys were rotated anyway.

`paths-mamba-CLAP.txt` (paths removed from every commit):
```text
target/
MambaDAW/build/
clap-plugin/build/
MambaDAW/bench_mamba
MambaDAW/test_mamba_layer
MambaDAW/tests/param_gate_test
MambaDAW/tools/benchmark
MambaDAW/python_training/resources/mamba_model.pt
MambaDAW/python_training/resources/mamba_model_final.pt
MambaDAW/python_training/resources/mamba_model_epoch_5.pt
MambaDAW/python_training/resources/mamba_model_epoch_10.pt
MambaDAW/python_training/resources/reconstructed_audio.wav
MambaDAW/resources/mamba_best.pt
MambaDAW/resources/mamba_final.pt
.vscode/
AGENTS.md
GEMINI.md
```

`replace.txt` is kept outside the repo, so the paths it removes aren't republished here. Use `/workspace/handoff/_history/replace-paths.txt` plus `replace-mamba-CLAP-extra.txt`. It has 8 literal lines covering the Linux, Fedora Atomic (`/var/home`), macOS and Windows home-directory prefixes and the `x:` workspace URI. The `/var/home` line comes before the plain `/home` line.

Commands:
```bash
# 1. Merge chore/sanitize into the default branch first, so the rewrite covers the cleaned tree.
git clone --mirror https://github.com/sgm-audio/mamba-CLAP.git mamba-CLAP-rewrite.git
cd mamba-CLAP-rewrite.git
git filter-repo --invert-paths --paths-from-file ../paths-mamba-CLAP.txt --replace-text ../replace.txt
# 2. Check before pushing
git log --all -p | grep -cEf ../path-patterns.txt   # expect 0 (patterns file is next to replace.txt)
gitleaks git . --no-banner --redact                                                       # expect no leaks
git count-objects -vH
# 3. Push. Needs Scott's OK: this rewrites every commit hash.
git push --force --all origin && git push --force --tags origin
```

Caveats:
- GitHub keeps `refs/pull/*` for every PR, and a force-push can't touch them. The old objects stay reachable through those PR refs and cached commit URLs until GitHub Support runs a GC; ask them to purge after the push.
- Forks and existing clones keep the old history. Collaborators need to re-clone, and open PRs have to be recreated.
- Most of the history size is a committed Rust `target/` directory (110+ large objects) and `MambaDAW/build/`.
- The `replace.txt` for this repo has one extra regex line that rewrites the placeholder token in old versions of `data-model.md`.
