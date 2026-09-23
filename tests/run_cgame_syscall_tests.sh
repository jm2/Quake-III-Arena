#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-cgame-syscalls.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

for Q3_TEST_FIXTURE in cgame_array_regression cgame_glconfig_regression; do
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined \
        "$Q3_TEST_ROOT/tests/$Q3_TEST_FIXTURE.c" \
        "$Q3_TEST_ROOT/code/qcommon/vm.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_FIXTURE"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_FIXTURE"
done
