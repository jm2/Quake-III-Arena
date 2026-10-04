#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-pc-handle-lifecycle.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -fsanitize=address,undefined)
Q3_TEST_OBJECTS=()
for Q3_TEST_SOURCE in l_precomp l_script; do
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" -DBOTLIB -fgnu89-inline \
        -c "$Q3_TEST_ROOT/code/botlib/$Q3_TEST_SOURCE.c" -o "$Q3_TEST_DIR/$Q3_TEST_SOURCE.o"
    Q3_TEST_OBJECTS+=("$Q3_TEST_DIR/$Q3_TEST_SOURCE.o")
done

for Q3_TEST_FIXTURE in CGAME UI GAME; do
    Q3_TEST_EXTRA=("$Q3_TEST_ROOT/code/qcommon/vm.c")
    if [[ "$Q3_TEST_FIXTURE" == GAME ]]; then
        Q3_TEST_EXTRA=("$Q3_TEST_ROOT/code/game/q_math.c")
    fi
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" "-DQ3_PC_FIXTURE_$Q3_TEST_FIXTURE" \
        "$Q3_TEST_ROOT/tests/pc_handle_lifecycle_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "${Q3_TEST_EXTRA[@]}" "${Q3_TEST_OBJECTS[@]}" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_FIXTURE"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_FIXTURE"
done
