#!/usr/bin/env bash
set -euo pipefail
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-vm-runtime.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/vm_runtime_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/vm_interpreted.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
