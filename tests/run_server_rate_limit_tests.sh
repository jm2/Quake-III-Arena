#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-server-rate-limit.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
    -Wno-pointer-to-int-cast -fsanitize=address,undefined)

# Issue #38: the fixture includes sv_main.c and drives SV_ConnectionlessPacket
# with a stubbed Sys_Milliseconds; the real message, Huffman, command-token and
# info-string code is linked beside it.
"${CC:-cc}" "${Q3_TEST_FLAGS[@]}" \
    "$Q3_TEST_ROOT/tests/server_rate_limit_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/msg.c" "$Q3_TEST_ROOT/code/qcommon/huffman.c" \
    "$Q3_TEST_ROOT/code/qcommon/cmd.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/server-rate-limit-tests"
# 0 normal use, 1 one source, 2 a second source, 3 many sources, 4 clock jumps
# and wrap, 5 bad rcon, 6 new sources and the table, 7 bounded bucket work.
for Q3_CASE in 0 1 2 3 4 5 6 7; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/server-rate-limit-tests" "$Q3_CASE"
done
