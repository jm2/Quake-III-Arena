#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-server-snapshot-budget.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
    -Wno-pointer-to-int-cast -fsanitize=address,undefined)

# Issue #438: real snapshot, download, client-command, netchan, message and Huffman code.
"${CC:-cc}" "${Q3_TEST_FLAGS[@]}" \
    "$Q3_TEST_ROOT/tests/server_snapshot_budget_regression.c" \
    "$Q3_TEST_ROOT/code/server/sv_snapshot.c" "$Q3_TEST_ROOT/code/server/sv_client.c" \
    "$Q3_TEST_ROOT/code/server/sv_net_chan.c" "$Q3_TEST_ROOT/code/qcommon/net_chan.c" \
    "$Q3_TEST_ROOT/code/qcommon/msg.c" "$Q3_TEST_ROOT/code/qcommon/huffman.c" \
    "$Q3_TEST_ROOT/code/qcommon/cmd.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/server-snapshot-budget-tests"
for Q3_CASE in 0 1 2 3 4; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/server-snapshot-budget-tests" "$Q3_CASE"
done
