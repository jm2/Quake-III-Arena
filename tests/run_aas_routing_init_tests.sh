#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_SOURCE="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-aas-routing-init.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_SOURCE"' EXIT
python3 - "$Q3_TEST_ROOT/code/botlib/be_aas_main.c" "$Q3_TEST_SOURCE/continuation.h" <<'PY_SOURCE'
from pathlib import Path
import sys
source=Path(sys.argv[1]).read_text();parts=[]
for name,signature in (('AAS_SetInitialized','void AAS_SetInitialized(void)'),('AAS_ContinueInit','void AAS_ContinueInit(float time)')):
    start=signature+'\n{';end='} //end of the function '+name
    assert source.count(start)==source.count(end+'\n')==1,('native initialization continuation seam changed',name)
    first=source.index(start);last=source.index(end,first)+len(end);parts.append(source[first:last])
Path(sys.argv[2]).write_text('\n'.join(parts)+'\n')
PY_SOURCE
for Q3_TEST_MODE in normal fast;do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]];then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math);fi
    "${CC:-cc}" -std=gnu99 -fgnu89-inline -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined,float-cast-overflow "${Q3_TEST_FLAGS[@]}" \
        "-DQ3_AAS_ROUTE_SOURCE=\"$Q3_TEST_ROOT/code/botlib/be_aas_route.c\"" \
        "-DQ3_AAS_INIT_CONTINUATION=\"$Q3_TEST_SOURCE/continuation.h\"" \
        "$Q3_TEST_ROOT/tests/aas_routing_init_regression.c" \
        "$Q3_TEST_ROOT/code/botlib/l_crc.c" "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_SOURCE/test"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_SOURCE/test"
done
