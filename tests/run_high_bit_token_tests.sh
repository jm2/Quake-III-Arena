#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-high-bit-token.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

# Issue #223: the host default and the Retro68 target flag (-fsigned-char)
# must both tokenize bytes >= 0x80 as retail 1.32c did.
for Q3_TEST_MODE in host target;do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == target ]];then Q3_TEST_FLAGS=(-fsigned-char);fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/high_bit_token_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
