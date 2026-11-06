#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Real game data cannot be committed, so this needs Q3_DEMO_PAK0 to name the
# Quake III demo's pak0.pk3, like the optional passes of run_vm_sandbox_tests.sh,
# or Q3_RETAIL_BASEQ3 to name a baseq3 with the retail paks. See
# real_content_boot.py. A big-endian run also needs OBJCOPY for that target.
if [[ -z "${Q3_DEMO_PAK0:-}" && -z "${Q3_RETAIL_BASEQ3:-}" ]]; then
    echo "real content boot skipped: neither Q3_DEMO_PAK0 nor Q3_RETAIL_BASEQ3 is set"
    exit 0
fi
exec python3 "$Q3_TEST_ROOT/tests/real_content_boot.py"
