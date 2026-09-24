#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-server-flood-protect.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# The fixture includes sv_client.c; the real message, Huffman and command-token
# code is linked beside it.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -Wno-pointer-to-int-cast -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/server_flood_protect_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/msg.c" \
    "$Q3_TEST_ROOT/code/qcommon/huffman.c" \
    "$Q3_TEST_ROOT/code/qcommon/cmd.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/server_flood_protect_regression"
# LeakSanitizer cannot initialize in the local ptrace sandbox.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/server_flood_protect_regression"
