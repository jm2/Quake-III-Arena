#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Real game data cannot be committed, so this needs Q3_RETAIL_BASEQ3 or
# Q3_DEMO_PAK0, as run_real_content_tests.sh does. Q3_PERF_REFERENCES names
# git refs to compare timings and outputs with; see map_load_perf.py.
if [[ -z "${Q3_DEMO_PAK0:-}" && -z "${Q3_RETAIL_BASEQ3:-}" ]]; then
    echo "map load timing skipped: neither Q3_DEMO_PAK0 nor Q3_RETAIL_BASEQ3 is set"
    exit 0
fi
exec python3 "$Q3_TEST_ROOT/tests/map_load_perf.py"
