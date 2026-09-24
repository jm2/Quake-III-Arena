#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-bot-chat-append.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

# Issue #306: also build with the Retro68 default unsigned char; the signed
# match offsets must read unset past byte 127 whatever plain char is.
for Q3_TEST_MODE in normal fast unsigned-char; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG); fi
    if [[ "$Q3_TEST_MODE" == unsigned-char ]]; then Q3_TEST_FLAGS=(-funsigned-char); fi
    "${CC:-cc}" -DBOTLIB -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/bot_chat_append_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
