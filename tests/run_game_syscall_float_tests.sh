#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-game-syscall-float.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT
for Q3_TEST_MODE in native static; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == static ]]; then
        Q3_TEST_FLAGS=(-DQ3_STATIC -DGAME_MODULE -DvmMain=Game_vmMain -DdllEntry=Game_dllEntry)
    fi
    "${CC:-cc}" -std=gnu99 -fgnu89-inline -fno-omit-frame-pointer \
        -fsanitize=address,undefined -I"$Q3_TEST_ROOT/code/qcommon" "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/game_syscall_float_regression.c" -lm -o "$Q3_TEST_BINARY"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
