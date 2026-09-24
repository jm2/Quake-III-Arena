#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-mover-constant-light.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Link the real native g_mover.c, g_spawn.c and g_utils.c, as the monolithic
# static build does, and spawn every InitMover class from an entity string.
# --gc-sections keeps only the mover spawn paths, whose engine references the
# test stubs. The test calls the mover spawn functions itself, so g_spawn.c's
# spawns[] table must be dropped too; AddressSanitizer's global registration
# would keep it (and every other entity's spawn function) alive, so g_spawn.c
# is built with UBSan only. shift and float-cast-overflow are named explicitly:
# they are the checks issues #344 and #373 are about, and GCC's
# -fsanitize=undefined leaves float-cast-overflow out. Built for base Quake III
# and Team Arena (MISSIONPACK).
for Q3_TEST_VARIANT in base missionpack; do
    Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
        -Wno-pointer-to-int-cast -fno-sanitize-recover=all)
    if [[ "$Q3_TEST_VARIANT" == missionpack ]]; then
        Q3_TEST_FLAGS+=(-DMISSIONPACK)
    fi
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" -fsanitize=undefined,shift,float-cast-overflow \
        -c "$Q3_TEST_ROOT/code/game/g_spawn.c" -o "$Q3_TEST_DIR/g_spawn-$Q3_TEST_VARIANT.o"
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" -fsanitize=address,undefined,shift,float-cast-overflow \
        "$Q3_TEST_ROOT/tests/mover_constant_light_regression.c" \
        "$Q3_TEST_ROOT/code/game/g_mover.c" \
        "$Q3_TEST_ROOT/code/game/g_utils.c" \
        "$Q3_TEST_ROOT/code/game/q_math.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        "$Q3_TEST_DIR/g_spawn-$Q3_TEST_VARIANT.o" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_VARIANT"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_VARIANT"
done
