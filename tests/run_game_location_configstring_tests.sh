#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-game-location-configstring.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Link the real native g_spawn.c, g_target.c, g_team.c, g_utils.c, g_mem.c
# and bg_misc.c, as the monolithic static build does, load maps with up to
# 958 target_locations through G_SpawnEntitiesFromString and the location
# linkup, and read the team overlay's locations through CheckTeamStatus.
# G_CallSpawn's spawns[] table names every entity's spawn function, so the
# test stubs those of the game files it does not link; --gc-sections drops
# the rest of the linked files' engine references. Built for base Quake III
# and Team Arena (MISSIONPACK).
for Q3_TEST_VARIANT in base missionpack; do
    Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
        -Wno-pointer-to-int-cast -fno-sanitize-recover=all)
    if [[ "$Q3_TEST_VARIANT" == missionpack ]]; then
        Q3_TEST_FLAGS+=(-DMISSIONPACK)
    fi
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" -fsanitize=address,undefined \
        "$Q3_TEST_ROOT/tests/game_location_configstring_regression.c" \
        "$Q3_TEST_ROOT/code/game/g_spawn.c" \
        "$Q3_TEST_ROOT/code/game/g_target.c" \
        "$Q3_TEST_ROOT/code/game/g_team.c" \
        "$Q3_TEST_ROOT/code/game/g_utils.c" \
        "$Q3_TEST_ROOT/code/game/g_mem.c" \
        "$Q3_TEST_ROOT/code/game/bg_misc.c" \
        "$Q3_TEST_ROOT/code/game/q_math.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_VARIANT"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_VARIANT"
done
