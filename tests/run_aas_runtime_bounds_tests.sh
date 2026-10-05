#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-aas-runtime-bounds.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
for Q3_TEST_FIXTURE in aas_point_index_regression aas_route_budget_regression server_bot_model_regression; do
    for Q3_TEST_MODE in normal fast; do
        Q3_TEST_FLAGS=()
        if [[ "$Q3_TEST_MODE" == fast ]]; then
            Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math)
        fi
        "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
            -fsanitize=address,undefined,float-cast-overflow "${Q3_TEST_FLAGS[@]}" \
            "$Q3_TEST_ROOT/tests/$Q3_TEST_FIXTURE.c" \
            "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
            -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_FIXTURE"
        # LeakSanitizer cannot initialize in the local ptrace sandbox.
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_FIXTURE"
    done
done
