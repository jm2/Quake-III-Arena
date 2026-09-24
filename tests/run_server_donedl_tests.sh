#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-server-donedl.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
    -Wno-pointer-to-int-cast -fsanitize=address,undefined)

# Real client-command, download, netchan-queue, snapshot, shutdown, message and Huffman code.
"${CC:-cc}" "${Q3_TEST_FLAGS[@]}" \
    "$Q3_TEST_ROOT/tests/server_donedl_regression.c" \
    "$Q3_TEST_ROOT/code/server/sv_client.c" "$Q3_TEST_ROOT/code/server/sv_net_chan.c" \
    "$Q3_TEST_ROOT/code/server/sv_snapshot.c" "$Q3_TEST_ROOT/code/server/sv_init.c" \
    "$Q3_TEST_ROOT/code/qcommon/msg.c" "$Q3_TEST_ROOT/code/qcommon/huffman.c" \
    "$Q3_TEST_ROOT/code/qcommon/cmd.c" "$Q3_TEST_ROOT/code/qcommon/net_chan.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/server-donedl-tests"
for Q3_CASE in 0 1 2 3 4 5 6 7 8 9 10 11; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/server-donedl-tests" "$Q3_CASE"
done
