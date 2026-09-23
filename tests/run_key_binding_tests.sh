#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-key-binding.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

# Issue #223: the Retro68 target now builds with -fsigned-char, where a
# high-bit key name used to become a negative keys[] index; keep the old
# unsigned-char target mode passing too.
for Q3_TEST_CHAR in -fsigned-char -funsigned-char; do
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "$Q3_TEST_CHAR" \
        "$Q3_TEST_ROOT/tests/key_binding_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/cmd.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
