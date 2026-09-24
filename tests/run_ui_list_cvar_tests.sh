#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-list-cvar.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #389: the menus' list selections are archived cvars, so a config or
# the console (or a server, before anything registers them) can set them to
# anything, and the Team Arena UI and q3_ui video menu index their lists with
# them. Each fixture sets the cvar through the real CL_SystemInfoChanged and
# cvar.c behind the module's real syscall layer, then drives every consumer.
# The UIs read some of the cvars back as floats; GCC's -fsanitize=undefined
# leaves out the float to int conversion check that Clang's includes.
Q3_TEST_ENGINE=(
    "$Q3_TEST_ROOT/tests/systeminfo_cvar_harness.c" "$Q3_TEST_ROOT/code/qcommon/cvar.c"
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c"
)
Q3_TEST_UI=()
for Q3_TEST_SOURCE in "$Q3_TEST_ROOT"/code/ui/ui_*.c; do
    # the fixture includes ui_main.c for its static items, feeders and scripts
    if [[ "$Q3_TEST_SOURCE" != */ui_main.c ]]; then
        Q3_TEST_UI+=("$Q3_TEST_SOURCE")
    fi
done
Q3_TEST_Q3UI=()
for Q3_TEST_SOURCE in "$Q3_TEST_ROOT"/code/q3_ui/ui_*.c; do
    # the fixture includes ui_video.c; CMake leaves the ranking menus out
    case "$Q3_TEST_SOURCE" in
        */ui_video.c|*/ui_rankings.c|*/ui_rankstatus.c|*/ui_signup.c|*/ui_login.c|*/ui_specifyleague.c) ;;
        *) Q3_TEST_Q3UI+=("$Q3_TEST_SOURCE") ;;
    esac
done
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined,float-cast-overflow -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/ui_list_cvar_regression.c" "${Q3_TEST_UI[@]}" \
    "$Q3_TEST_ROOT/code/game/bg_misc.c" "${Q3_TEST_ENGINE[@]}" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui"
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined,float-cast-overflow \
    "$Q3_TEST_ROOT/tests/q3ui_video_regression.c" "${Q3_TEST_Q3UI[@]}" \
    "$Q3_TEST_ROOT/code/game/bg_misc.c" "${Q3_TEST_ENGINE[@]}" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/q3ui"

# One process per cvar and value. Each list's entries must act as they always
# did; -1, the list's count, INT_MAX (read back as a float, 2^31, past the int
# range) and INT_MIN name no entry and must act as the entry the menu shows.
# LeakSanitizer cannot initialize in the local ptrace sandbox.
Q3_TEST_RUN=(env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1)
Q3_TEST_EDGES=(2147483647 -2147483648)
# the fixture's 8 game types and 8 browser game types
for Q3_TEST_CVAR in ui_gameType ui_netGameType ui_joinGameType; do
    for Q3_TEST_VALUE in -1 0 1 3 7 8 "${Q3_TEST_EDGES[@]}"; do
        "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" "$Q3_TEST_CVAR" "$Q3_TEST_VALUE"
    done
done
# the fixture's 4 maps
for Q3_TEST_CVAR in ui_currentMap ui_currentNetMap; do
    for Q3_TEST_VALUE in -1 0 1 2 3 4 "${Q3_TEST_EDGES[@]}"; do
        "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" "$Q3_TEST_CVAR" "$Q3_TEST_VALUE"
    done
done
# the 4 server sources, and the 7 mod filters (nothing registers
# ui_serverFilterType, as in retail, so the fixture sets its vmCvar as the
# filter item does)
for Q3_TEST_VALUE in -1 0 1 2 3 4 "${Q3_TEST_EDGES[@]}"; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" ui_netSource "$Q3_TEST_VALUE"
done
for Q3_TEST_VALUE in -1 0 1 5 6 7 "${Q3_TEST_EDGES[@]}"; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" ui_serverFilterType "$Q3_TEST_VALUE"
done
# no tiers; ui_currentMap is also a tier's map, of 3 (MAPS_PER_TIER)
for Q3_TEST_VALUE in -1 0 1 "${Q3_TEST_EDGES[@]}"; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" ui_currentTier "$Q3_TEST_VALUE"
done
# skill levels 1 to 5
for Q3_TEST_VALUE in -1 0 1 3 5 6 "${Q3_TEST_EDGES[@]}"; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" g_spSkill "$Q3_TEST_VALUE"
done
# a team game's slots: closed, human and the fixture's 3 characters (0 to 4)
for Q3_TEST_CVAR in ui_blueteam1 ui_redteam1; do
    for Q3_TEST_VALUE in -1 0 1 2 4 5 "${Q3_TEST_EDGES[@]}"; do
        "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" "$Q3_TEST_CVAR" "$Q3_TEST_VALUE"
    done
done
# 10 crosshairs
for Q3_TEST_VALUE in -1 0 4 9 10 12 "${Q3_TEST_EDGES[@]}"; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" cg_drawCrosshair "$Q3_TEST_VALUE"
done
# the 2 skirmish maps Team Deathmatch lists
for Q3_TEST_VALUE in -1 0 1 2 "${Q3_TEST_EDGES[@]}"; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" ui_mapIndex "$Q3_TEST_VALUE"
done
# the server's g_gametype, against a map's 16 (MAX_GAMETYPES) times to beat
for Q3_TEST_VALUE in -1 0 3 7 15 16 "${Q3_TEST_EDGES[@]}"; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" postgame "$Q3_TEST_VALUE"
done
# no gameinfo.txt yet: every game type selection names no entry, and the
# saved map (2) must survive until a menu loads the list
for Q3_TEST_VALUE in -1 0 3 "${Q3_TEST_EDGES[@]}"; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" emptyLists "$Q3_TEST_VALUE"
done
# q3_ui's 12 video modes, and the fullscreen and extensions switches
for Q3_TEST_VALUE in -2 -1 0 3 11 12 13 "${Q3_TEST_EDGES[@]}"; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/q3ui" r_mode "$Q3_TEST_VALUE"
done
for Q3_TEST_CVAR in r_fullscreen r_allowExtensions; do
    for Q3_TEST_VALUE in -1 0 1 2 3 "${Q3_TEST_EDGES[@]}"; do
        "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/q3ui" "$Q3_TEST_CVAR" "$Q3_TEST_VALUE"
    done
done
