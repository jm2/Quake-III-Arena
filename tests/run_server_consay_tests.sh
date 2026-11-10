#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-server-consay.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# SV_ConSay_f from sv_ccmds.c verbatim (the rest needs the whole server),
# with the real Cmd_TokenizeString and Cmd_Args from cmd.c.
awk -v file="$Q3_TEST_ROOT/code/server/sv_ccmds.c" '
    /^static void SV_ConSay_f[(]void[)] [{]$/ { printf "#line %d \"%s\"\n", NR, file; found = 1 }
    found { print }
    found && /^}/ { found = 0; count++ }
    END { exit count == 1 ? 0 : 1 }
' "$Q3_TEST_ROOT/code/server/sv_ccmds.c" > "$Q3_TEST_DIR/server_consay_extracted.c" ||
    { echo "run_server_consay_tests: no single SV_ConSay_f in sv_ccmds.c" >&2; exit 1; }

"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-sanitize-recover=all -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/server_consay_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/cmd.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/server_consay"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/server_consay"
