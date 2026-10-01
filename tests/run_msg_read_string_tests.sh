#!/usr/bin/env bash
# Issue #435: MSG_ReadString, MSG_ReadBigString and MSG_ReadStringLine must
# read on to a string's terminator even when it fills their buffer, and read
# every string retail 1.32c reads whole exactly as retail does.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-msg-read-string.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -Wno-pointer-to-int-cast \
    -fsanitize=address,undefined "$Q3_TEST_ROOT/tests/msg_read_string_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/msg.c" "$Q3_TEST_ROOT/code/qcommon/huffman.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

# LeakSanitizer cannot initialize in the local ptrace sandbox.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
