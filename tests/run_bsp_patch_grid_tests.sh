#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-bsp-patch-grid.XXXXXX")"
Q3_TEST_PATCH_OBJECT="$(mktemp "${TMPDIR:-/var/tmp}/q3-bsp-patch-object.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY" "$Q3_TEST_PATCH_OBJECT"' EXIT
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -Dmalloc=PatchGridMalloc -Dfree=PatchGridFree \
    -c "$Q3_TEST_ROOT/code/qcommon/cm_patch.c" -o "$Q3_TEST_PATCH_OBJECT"
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined "$Q3_TEST_ROOT/tests/bsp_patch_grid_regression.c" "$Q3_TEST_PATCH_OBJECT" \
    "$Q3_TEST_ROOT/code/qcommon/cm_test.c" "$Q3_TEST_ROOT/code/qcommon/cm_polylib.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
