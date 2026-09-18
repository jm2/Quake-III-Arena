#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-bot-memory.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT
for Q3_TEST_MODE in native debug tracked tracked-debug; do
    Q3_TEST_FLAGS=()
    case "$Q3_TEST_MODE" in
        debug) Q3_TEST_FLAGS=(-DMEMDEBUG) ;;
        tracked) Q3_TEST_FLAGS=(-DMEMORYMANEGER) ;;
        tracked-debug) Q3_TEST_FLAGS=(-DMEMORYMANEGER -DMEMDEBUG) ;;
    esac
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/bot_memory_regression.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
