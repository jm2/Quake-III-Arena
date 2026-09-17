#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-botlib-actions.XXXXXX)"
Q3_TEST_BINARY="$Q3_TEST_DIR/actions"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/botlib_actions_regression.c" "$Q3_TEST_ROOT/code/botlib/be_ea.c" \
    "$Q3_TEST_ROOT/code/qcommon/vm.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/server_bot_client_regression.c" "$Q3_TEST_ROOT/code/server/sv_game.c" \
    "$Q3_TEST_ROOT/code/qcommon/vm.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/clients"

# LeakSanitizer cannot initialize in the local ptrace sandbox.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/clients"
