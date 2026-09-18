#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_SOURCE="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-aas-routing-time.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_SOURCE"' EXIT
# Supply only cache-provider seams; the native routing consumers stay unchanged.
python3 - "$Q3_TEST_ROOT/code/botlib/be_aas_route.c" "$Q3_TEST_SOURCE/route.c" <<'PY_SOURCE'
from pathlib import Path
import re,sys
source=Path(sys.argv[1]).read_text()
main=Path(sys.argv[1]).with_name('be_aas_main.c').read_text()
start='void AAS_ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj )\n{'
end='} //end of the function AAS_ProjectPointOntoVector'
assert main.count(start)==main.count(end)==1,'native projection seam changed'
first=main.index(start);last=main.index(end,first)+len(end)
source+='\n'+main[first:last]+'\n'
for name in ('AAS_GetAreaRoutingCache','AAS_GetPortalRoutingCache'):
    source,count=re.subn(r'(?m)^(aas_routingcache_t \*)'+name+r'(\()',r'\1'+name+r'(int, int, int);\n\1FixtureUnused_'+name+r'\2',source)
    assert count==1,('native cache-provider seam changed',name,count)
Path(sys.argv[2]).write_text(source)
PY_SOURCE
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math); fi
    "${CC:-cc}" -std=gnu99 -fgnu89-inline -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined,float-cast-overflow "${Q3_TEST_FLAGS[@]}" -I"$Q3_TEST_ROOT/code/botlib" \
        "-DQ3_AAS_TIME_ROUTE_SOURCE=\"$Q3_TEST_SOURCE/route.c\"" \
        "$Q3_TEST_ROOT/tests/aas_routing_time_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_SOURCE/test"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_SOURCE/test"
done
