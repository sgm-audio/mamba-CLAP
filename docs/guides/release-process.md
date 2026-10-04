# Release process — proposed checklist

> **Status (2026-10-04):** no production plug-in or service release process is established. This checklist is prospective; it does not imply that builds, CI, signing, distribution, or support infrastructure are configured. See [`../PROJECT_STATUS.md`](../PROJECT_STATUS.md) and [`../BUILD_AND_TEST_GUIDE.md`](../BUILD_AND_TEST_GUIDE.md).

## Release gates

Do not create a product release until the chosen component has:

1. A clean, reproducible build from the documented source revision and dependencies.
2. Tests that run in CI and report their actual status; the current `scripts/build-all.sh` is disabled and no root `.github/workflows/ci.yml` is present.
3. Host/format validation for each advertised plug-in format and platform.
4. A matched model/checkpoint/export contract, when ML is included, plus output parity tests.
5. Worst-case callback/allocation measurements and audio-quality/listening evidence.
6. Reviewed dependencies, licenses, signing/notarization, installation, uninstall, state migration, and user support.
7. Separate evidence for customer demand, operating costs, and pricing; technical readiness does not prove a business forecast.

The current repository passes none of these release gates as a complete product. The standalone custom C++ layer test is narrow and is not a release test.

## Prospective versioning and packaging

Semantic versioning, component-specific releases, changelog entries, signed artifacts, and platform-specific packages are possible future policies, not policies already established for this checkout. Upstream `mamba/` release infrastructure is not a release workflow for this repository's plug-ins or SaaS concepts.

When a release path is ready, document the chosen component, version source, exact build/test commands, artifact checksums, supported hosts/platforms, rollback plan, and publication owner. Do not assume that a root `setup.py`, CI workflow, PyPI package, or signed plug-in artifact exists; inspect the current tree first.
