#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-bot-libvar.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT
for Q3_TEST_MODE in normal fast;do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]];then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math);fi
    "${CC:-cc}" -std=gnu99 -fgnu89-inline -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined,float-cast-overflow "${Q3_TEST_FLAGS[@]}" \
        "-DQ3_LIBVAR_SOURCE=\"$Q3_TEST_ROOT/code/botlib/l_libvar.c\"" \
        "$Q3_TEST_ROOT/tests/bot_libvar_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
