#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_WORK="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-cvar-systeminfo.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_WORK"' EXIT
# Issue #303: take the systeminfo cvars from SV_Init itself, so the test sees
# the engine's real creation order (Cvar_InfoString_Big walks newest first).
python3 - "$Q3_TEST_ROOT/code/server/sv_init.c" "$Q3_TEST_WORK/sv_systeminfo_cvars.c" <<'PY_SOURCE'
import re, sys
from pathlib import Path
matches = re.findall(r'^\t// systeminfo\n(.*?^\tCvar_Get \("sv_referencedPakNames",[^\n]*\n)',
                     Path(sys.argv[1]).read_text(), re.M | re.S)
if len(matches) != 1:
    raise SystemExit("SV_Init systeminfo cvar extraction seam no longer matches")
Path(sys.argv[2]).write_text(matches[0])
PY_SOURCE
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined \
    -I"$Q3_TEST_ROOT/code/game" -I"$Q3_TEST_ROOT/code/qcommon" \
    "-DQ3_SV_SYSTEMINFO_CVARS=\"$Q3_TEST_WORK/sv_systeminfo_cvars.c\"" \
    "$Q3_TEST_ROOT/tests/cvar_systeminfo_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/cvar.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_WORK/test"
# LeakSanitizer cannot initialize under the ptrace-based local worker sandbox.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_WORK/test"
