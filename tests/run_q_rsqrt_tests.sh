#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-q-rsqrt.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Q_rsqrt's float bits must be read as 32 bits on every host (see the test).
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -fno-strict-aliasing -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/q_rsqrt_regression.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" -lm -o "$Q3_TEST_DIR/q_rsqrt"
ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/q_rsqrt"
