#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-bsp-trace.XXXXXX")"
Q3_TEST_TRACE_OBJECT="$(mktemp "${TMPDIR:-/var/tmp}/q3-bsp-trace-object.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY" "$Q3_TEST_TRACE_OBJECT"' EXIT
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -Dmalloc=TraceMalloc -Dfree=TraceFree \
    -c "$Q3_TEST_ROOT/code/qcommon/cm_trace.c" -o "$Q3_TEST_TRACE_OBJECT"
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined "$Q3_TEST_ROOT/tests/bsp_trace_regression.c" "$Q3_TEST_TRACE_OBJECT" \
    "$Q3_TEST_ROOT/code/qcommon/cm_test.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
(
    ulimit -s 512
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
)
