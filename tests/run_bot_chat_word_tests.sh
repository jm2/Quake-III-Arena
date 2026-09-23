#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-bot-chat-word.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG); fi
    "${CC:-cc}" -DBOTLIB -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/bot_chat_word_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    # The 1.32 search oracle catches its own reads at a guard page; LeakSanitizer
    # cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:handle_segv=0:handle_sigbus=0:allow_user_segv_handler=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
