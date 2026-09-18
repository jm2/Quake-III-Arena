#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_SOURCE="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-aas-travel-source.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_SOURCE"' EXIT
python3 - "$Q3_TEST_ROOT" "$Q3_TEST_SOURCE/body.h" <<'PY_SOURCE'
from pathlib import Path
import re,sys
root,out=map(Path,sys.argv[1:])
route=(root/'code/botlib/be_aas_route.c').read_text()
reach=(root/'code/botlib/be_aas_reach.c').read_text()
parts=re.findall(r'(?m)^#define DISTANCEFACTOR_.*$',route)
assert len(parts)==3,'native distance-factor seam changed'
start='static unsigned short AAS_TravelTimeFromFloat(float value)\n{'
assert route.count(start)==1,'native float-time seam changed'
first=route.index(start);last=route.index('\n}',first)+2
parts.append(route[first:last])
for source,name,signature in (
    (reach,'AAS_AreaCrouch','int AAS_AreaCrouch(int areanum)'),
    (reach,'AAS_AreaSwim','int AAS_AreaSwim(int areanum)'),
    (route,'AAS_AreaTravelTime','unsigned short int AAS_AreaTravelTime(int areanum, vec3_t start, vec3_t end)'),
):
    start=signature+'\n{';end='} //end of the function '+name
    assert source.count(start)==source.count(end+"\n")==1,('native travel seam changed',name)
    first=source.index(start);last=source.index(end,first)+len(end)
    parts.append(source[first:last])
out.write_text('\n'.join(parts)+'\n')
PY_SOURCE
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined,float-cast-overflow "${Q3_TEST_FLAGS[@]}" \
        "-DQ3_AAS_TRAVEL_BODY=\"$Q3_TEST_SOURCE/body.h\"" \
        "$Q3_TEST_ROOT/tests/aas_travel_time_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_SOURCE/test"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_SOURCE/test"
done
