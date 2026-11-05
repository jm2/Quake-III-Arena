#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Real game data cannot be committed, so this needs Q3_DEMO_PAK0 to name the
# Quake III demo's pak0.pk3, like the optional passes of run_vm_sandbox_tests.sh.
# CC takes tests/be32_cc.sh for a big-endian run. See real_content_boot.py.
if [[ -z "${Q3_DEMO_PAK0:-}" ]]; then
    echo "real content boot skipped: Q3_DEMO_PAK0 is not set"
    exit 0
fi
exec python3 "$Q3_TEST_ROOT/tests/real_content_boot.py"
