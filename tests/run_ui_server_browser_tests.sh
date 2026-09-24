#!/usr/bin/env bash
# Issue #40: the Team Arena (code/ui, MISSIONPACK) and base q3_ui server
# browsers are linked natively; the fixtures include the real ui_main.c and
# ui_servers2.c and feed them hostile server, master and status replies.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-ui-server-browser.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
for Q3_TEST_CASE in "ui_server_browser -DMISSIONPACK" "q3ui_server_browser"; do
    # shellcheck disable=SC2086 # the case is a fixture and its flags
    set -- $Q3_TEST_CASE
    Q3_TEST_NAME="$1"
    shift
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "$@" \
        "$Q3_TEST_ROOT/tests/${Q3_TEST_NAME}_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_NAME"
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_NAME"
done
