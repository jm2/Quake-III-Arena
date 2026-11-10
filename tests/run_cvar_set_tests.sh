#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-cvar-set.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Cvar_Set_f from cvar.c verbatim (the rest of cvar.c needs the whole
# engine), with the real Cmd_TokenizeString and Cmd_Argv from cmd.c.
awk -v file="$Q3_TEST_ROOT/code/qcommon/cvar.c" '
    /^void Cvar_Set_f[(] void [)] [{]$/ { printf "#line %d \"%s\"\n", NR, file; found = 1 }
    found { print }
    found && /^}/ { found = 0; count++ }
    END { exit count == 1 ? 0 : 1 }
' "$Q3_TEST_ROOT/code/qcommon/cvar.c" > "$Q3_TEST_DIR/cvar_set_extracted.c" ||
    { echo "run_cvar_set_tests: no single Cvar_Set_f in cvar.c" >&2; exit 1; }

"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-sanitize-recover=all -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/cvar_set_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/cmd.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/cvar_set"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/cvar_set"
