#!/usr/bin/env bash
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-snd-resample.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

# The test includes the real snd_mem.c and loads 8-bit WAV files through
# S_LoadSound and both resamplers. shift is named explicitly: it is the check
# issue #344 is about.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined,shift -fno-sanitize-recover=all \
    "$Q3_TEST_ROOT/tests/snd_resample_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

# LeakSanitizer cannot initialize in the local ptrace sandbox.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
