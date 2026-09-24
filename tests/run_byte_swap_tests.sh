#!/usr/bin/env bash
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-byte-swap.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

# shift-base is named explicitly: it is the check issue #333 is about.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined,shift-base -fno-sanitize-recover=all \
    "$Q3_TEST_ROOT/tests/byte_swap_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

# Each mode runs in its own process so a report in one does not hide the other.
for Q3_TEST_MODE in swap wav; do
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY" "$Q3_TEST_MODE"
done
