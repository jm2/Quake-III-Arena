#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-toss-weapon.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Link the real native TossClientItems (g_combat.c) and BG_FindItemForWeapon
# (bg_misc.c), as the monolithic static build does, and drive every usercmd
# weapon byte through it. --gc-sections keeps only the tested paths, whose sole
# engine references (Com_Error, Drop_Item, g_gametype, level) the test stubs.
# Built for base Quake III and Team Arena (MISSIONPACK).
for Q3_TEST_VARIANT in base missionpack; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_VARIANT" == missionpack ]]; then
        Q3_TEST_FLAGS=(-DMISSIONPACK)
    fi
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/toss_client_weapon_regression.c" \
        "$Q3_TEST_ROOT/code/game/g_combat.c" \
        "$Q3_TEST_ROOT/code/game/bg_misc.c" \
        "$Q3_TEST_ROOT/code/game/q_math.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_VARIANT"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_VARIANT"
done
