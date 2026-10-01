#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-game-configstring-transport.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Link the real native g_cmds.c, g_client.c, g_target.c, g_spawn.c, g_utils.c
# and g_mem.c, as the monolithic static build does, and set the configstrings
# they build from map and client text: SP_worldspawn, target_location and
# target_speaker from map entity strings, Cmd_CallVote_f, Cmd_CallTeamVote_f
# and ClientUserinfoChanged. --gc-sections keeps only those paths, whose
# engine and game references the test stubs. g_spawn.c's spawns[] table must
# be dropped too; AddressSanitizer's global registration would keep it (and
# every entity's spawn function) alive, so g_spawn.c is built with UBSan
# only. Built for base Quake III and Team Arena (MISSIONPACK).
for Q3_TEST_VARIANT in base missionpack; do
    Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
        -Wno-pointer-to-int-cast -fno-sanitize-recover=all)
    if [[ "$Q3_TEST_VARIANT" == missionpack ]]; then
        Q3_TEST_FLAGS+=(-DMISSIONPACK)
    fi
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" -fsanitize=undefined \
        -c "$Q3_TEST_ROOT/code/game/g_spawn.c" -o "$Q3_TEST_DIR/g_spawn-$Q3_TEST_VARIANT.o"
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" -fsanitize=address,undefined \
        "$Q3_TEST_ROOT/tests/game_configstring_transport_regression.c" \
        "$Q3_TEST_ROOT/code/game/g_cmds.c" \
        "$Q3_TEST_ROOT/code/game/g_client.c" \
        "$Q3_TEST_ROOT/code/game/g_target.c" \
        "$Q3_TEST_ROOT/code/game/g_utils.c" \
        "$Q3_TEST_ROOT/code/game/g_mem.c" \
        "$Q3_TEST_ROOT/code/game/q_math.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        "$Q3_TEST_DIR/g_spawn-$Q3_TEST_VARIANT.o" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_VARIANT"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_VARIANT"
done
