#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-bsp-box-identity.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT
# Sanitized at -O0, then optimized, since the side cache is plain bit arithmetic.
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" "$Q3_TEST_ROOT/tests/bsp_box_query_identity_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/cm_test.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    (
        ulimit -s 512
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
    )
done
