#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-client-oob-sender.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -Wno-pointer-to-int-cast \
    -fsanitize=address,undefined "$Q3_TEST_ROOT/tests/client_oob_sender_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/net_chan.c" "$Q3_TEST_ROOT/code/qcommon/msg.c" \
    "$Q3_TEST_ROOT/code/qcommon/cmd.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

# LeakSanitizer cannot initialize in the local ptrace sandbox.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
