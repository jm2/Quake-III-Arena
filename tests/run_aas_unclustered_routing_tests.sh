#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_SOURCE="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-aas-unclustered.XXXXXX")"
Q3_TEST_BINARY="$Q3_TEST_SOURCE/test"
trap 'rm -rf -- "$Q3_TEST_SOURCE"' EXIT
# These legacy headers lack guards; reuse the route translation unit's types.
python3 - "$Q3_TEST_ROOT/code/botlib/be_aas_cluster.c" "$Q3_TEST_SOURCE/cluster.c" <<'PY_SOURCE'
from pathlib import Path
import re,sys
source=Path(sys.argv[1]).read_text()
source,count=re.subn(r'(?m)^#include "[^"]+"\n','',source)
assert count==13,'native clustering header seam changed'
Path(sys.argv[2]).write_text(source)
PY_SOURCE
for Q3_TEST_MODE in normal fast;do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]];then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math);fi
    "${CC:-cc}" -std=gnu99 -fgnu89-inline -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "-DQ3_AAS_ROUTE_SOURCE=\"$Q3_TEST_ROOT/code/botlib/be_aas_route.c\"" \
        "-DQ3_AAS_CLUSTER_SOURCE=\"$Q3_TEST_SOURCE/cluster.c\"" \
        "$Q3_TEST_ROOT/tests/aas_unclustered_routing_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
