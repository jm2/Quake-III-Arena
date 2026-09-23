#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_WORK="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-info-string.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_WORK"' EXIT
# Extract the native Info_Print so it prints through the test's capturing Com_Printf.
python3 - "$Q3_TEST_ROOT/code/qcommon/common.c" "$Q3_TEST_WORK/info_print.c" <<'PY_SOURCE'
import re, sys
from pathlib import Path
matches = re.findall(r'^void Info_Print\( const char \*s \) \{.*?^\}', Path(sys.argv[1]).read_text(), re.M | re.S)
if len(matches) != 1:
    raise SystemExit("Native Info_Print extraction seam no longer matches")
Path(sys.argv[2]).write_text(matches[0] + "\n")
PY_SOURCE
for Q3_TEST_OPTIMIZATION in normal optimized; do
    Q3_TEST_OPT_FLAGS=()
    if [[ "$Q3_TEST_OPTIMIZATION" == optimized ]]; then Q3_TEST_OPT_FLAGS=(-O2 -DNDEBUG); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_OPT_FLAGS[@]}" \
        -I"$Q3_TEST_ROOT/code/game" "-DQ3_INFO_PRINT_SOURCE=\"$Q3_TEST_WORK/info_print.c\"" \
        "$Q3_TEST_ROOT/tests/info_string_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_WORK/test"
    # LeakSanitizer cannot initialize under the ptrace-based local worker sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_WORK/test"
done
