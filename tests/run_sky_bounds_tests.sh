#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-sky-bounds.XXXXXX")"
Q3_SKY_COMMON_MATH="$(mktemp "${TMPDIR:-/var/tmp}/q3-sky-common-math.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY" "$Q3_SKY_COMMON_MATH"' EXIT
python - "$Q3_TEST_ROOT/code/qcommon/common.c" "$Q3_SKY_COMMON_MATH" <<'PYCODE'
from pathlib import Path
import re
import sys
matches=re.findall(r'^float Q_acos\(float c\) \{.*?^\}',Path(sys.argv[1]).read_text(),re.M|re.S)
if len(matches)!=1:
    raise SystemExit("Native Q_acos extraction seam no longer matches")
Path(sys.argv[2]).write_text(matches[0]+"\n")
PYCODE
for Q3_SHADER_CONFIGURATION in sanitizer release-fast-math; do
    Q3_SHADER_FLAGS=()
    if [ "$Q3_SHADER_CONFIGURATION" = release-fast-math ]; then
        Q3_SHADER_FLAGS=(-O2 -DNDEBUG -ffast-math)
    fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        "${Q3_SHADER_FLAGS[@]}" -DQ3_SKY_COMMON_MATH=\""$Q3_SKY_COMMON_MATH"\" -fsanitize=address,undefined,float-cast-overflow \
        "$Q3_TEST_ROOT/tests/sky_bounds_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
