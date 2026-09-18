#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-aas-node.XXXXXX")"
Q3_TEST_SOURCE="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-aas-node-source.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"; rm -rf -- "$Q3_TEST_SOURCE"' EXIT
python3 - "$Q3_TEST_ROOT/code/botlib/be_aas_sample.c" "$Q3_TEST_SOURCE/point-body.h" <<'Q3_POINT_PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
start = "int AAS_PointAreaNum(vec3_t point)\n{"
end = "} //end of the function AAS_PointAreaNum"
assert source.count(start) == source.count(end) == 1, "native point-query seam changed"
first = source.index(start)
last = source.index(end, first) + len(end)
Path(sys.argv[2]).write_text(source[first:last] + "\n")
Q3_POINT_PY
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then
        Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math)
    fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined,float-cast-overflow "${Q3_TEST_FLAGS[@]}" \
        "-DQ3_AAS_POINT_BODY=\"$Q3_TEST_SOURCE/point-body.h\"" \
        "$Q3_TEST_ROOT/tests/aas_node_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
