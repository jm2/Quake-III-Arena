#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-shader-runtime.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT
for Q3_SHADER_CONFIGURATION in sanitizer release-fast-math; do
    Q3_SHADER_FLAGS=()
    if [ "$Q3_SHADER_CONFIGURATION" = release-fast-math ]; then
        Q3_SHADER_FLAGS=(-O2 -DNDEBUG -ffast-math)
    fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        "${Q3_SHADER_FLAGS[@]}" -fsanitize=address,undefined,float-cast-overflow \
        "$Q3_TEST_ROOT/tests/shader_runtime_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
