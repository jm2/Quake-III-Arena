#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_SOURCE="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-aas-volume-source.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_SOURCE"' EXIT
python3 - "$Q3_TEST_ROOT" "$Q3_TEST_SOURCE/body.h" <<'PY_SOURCE'
from pathlib import Path
import sys
root,out=map(Path,sys.argv[1:])
reach=(root/'code/botlib/be_aas_reach.c').read_text()
parts=[]
for name,signature in (
    ('AAS_FaceArea','float AAS_FaceArea(aas_face_t *face)'),
    ('AAS_AreaVolume','float AAS_AreaVolume(int areanum)'),
):
    start=signature+'\n{';end='} //end of the function '+name
    assert reach.count(start)==reach.count(end+"\n")==1,('native volume seam changed',name)
    first=reach.index(start);last=reach.index(end,first)+len(end)
    parts.append(reach[first:last])
out.write_text('\n'.join(parts)+'\n')
PY_SOURCE
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined "-DQ3_AAS_VOLUME_BODY=\"$Q3_TEST_SOURCE/body.h\"" \
    "$Q3_TEST_ROOT/tests/aas_area_volume_regression.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_SOURCE/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_SOURCE/test"
