#!/usr/bin/env bash
# Deprecated legacy entry point. The repository has no verified all-component
# build, and the old script could skip missing components then print success.
set -euo pipefail

echo "ERROR: scripts/build-all.sh is disabled; no verified all-component build exists." >&2
echo "See docs/BUILD_AND_TEST_GUIDE.md and docs/PROJECT_STATUS.md for the current build blockers." >&2
exit 2
