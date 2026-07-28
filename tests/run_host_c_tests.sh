#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/tmp}/q3-q-shared-regression.XXXXXX")"
Q3_TEST_CC="${CC:-cc}"

cleanup() {
    rm -f -- "$Q3_TEST_BINARY"
}
trap cleanup EXIT

"$Q3_TEST_CC" \
    -std=gnu99 \
    -fno-omit-frame-pointer \
    -ffunction-sections \
    -fdata-sections \
    -fsanitize=address,undefined \
    -I"$Q3_TEST_ROOT/code/game" \
    "$Q3_TEST_ROOT/tests/q_shared_regression.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections \
    -lm \
    -o "$Q3_TEST_BINARY"

# LeakSanitizer cannot initialize under the ptrace-based local worker
# sandbox. This harness exercises stack/bounds behavior and does not allocate.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_BINARY"
