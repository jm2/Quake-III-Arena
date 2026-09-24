#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-cvar-index.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #379: a server can set any client cvar through systeminfo. The Team
# Arena UI and cgame (MISSIONPACK) and the base q3_ui are linked natively; each
# fixture runs the real CL_SystemInfoChanged and cvar.c behind the module's
# real syscall stubs, so the modules read the server's value back through
# trap_Cvar_Update and trap_Cvar_VariableValue as they do in the client.
# The UIs read some of the cvars back as floats; GCC's -fsanitize=undefined
# leaves out the float to int conversion check that Clang's includes.
# Issue #390: the Team Arena UI fixture routes ui_main.c's va and Com_sprintf
# through printf-attributed checkers, so -Wformat checks their literal formats
# and the checkers check the menus' orders strings at run time. ui_main.c passes
# one extra argument, which printf ignores.
# Issue #401: a pk3, even a downloaded one, can replace the menus, so the orders
# and voiceOrders scripts must refuse a string that is not exactly one plain %i
# or %d before it reaches a formatter.
Q3_TEST_ENGINE=(
    "$Q3_TEST_ROOT/tests/systeminfo_cvar_harness.c" "$Q3_TEST_ROOT/code/qcommon/cvar.c"
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c"
)
Q3_TEST_UI=()
for Q3_TEST_SOURCE in "$Q3_TEST_ROOT"/code/ui/ui_*.c; do
    # the fixture includes ui_main.c for its static menu script and item handlers
    if [[ "$Q3_TEST_SOURCE" != */ui_main.c ]]; then
        Q3_TEST_UI+=("$Q3_TEST_SOURCE")
    fi
done
Q3_TEST_Q3UI=()
for Q3_TEST_SOURCE in "$Q3_TEST_ROOT"/code/q3_ui/ui_*.c; do
    # the fixture includes ui_preferences.c; CMake leaves the ranking menus out
    case "$Q3_TEST_SOURCE" in
        */ui_preferences.c|*/ui_rankings.c|*/ui_rankstatus.c|*/ui_signup.c|*/ui_login.c|*/ui_specifyleague.c) ;;
        *) Q3_TEST_Q3UI+=("$Q3_TEST_SOURCE") ;;
    esac
done
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined,float-cast-overflow -DMISSIONPACK -Wformat -Werror=format -Wno-format-extra-args \
    "$Q3_TEST_ROOT/tests/ui_cvar_index_regression.c" "${Q3_TEST_UI[@]}" \
    "$Q3_TEST_ROOT/code/game/bg_misc.c" "${Q3_TEST_ENGINE[@]}" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui"
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/cgame_selected_player_regression.c" "$Q3_TEST_ROOT"/code/cgame/cg_*.c \
    "$Q3_TEST_ROOT/code/ui/ui_shared.c" "$Q3_TEST_ROOT/code/game/bg_misc.c" \
    "$Q3_TEST_ROOT/code/game/bg_pmove.c" "$Q3_TEST_ROOT/code/game/bg_slidemove.c" "${Q3_TEST_ENGINE[@]}" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/cgame"
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined,float-cast-overflow \
    "$Q3_TEST_ROOT/tests/q3ui_crosshair_regression.c" "${Q3_TEST_Q3UI[@]}" \
    "$Q3_TEST_ROOT/code/game/bg_misc.c" "${Q3_TEST_ENGINE[@]}" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/q3ui"

# One process per value. Each list's valid entries and its count ("Everyone"
# for the selected players) must act as they always did; -1 and INT_MIN are
# below every list, INT_MAX is also past the int range once read back as a
# float (2^31), and a full 32-player team overlay's count is one past
# sortedTeamPlayers.
# LeakSanitizer cannot initialize in the local ptrace sandbox.
Q3_TEST_RUN=(env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1)
# Everyone (3) and the values that name nobody send the orders script's
# commands to every teammate but the local client (issue #390).
for Q3_TEST_VALUE in -1 0 1 2 3 2147483647 -2147483648; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" selected "$Q3_TEST_VALUE"
done
for Q3_TEST_TEAM in 3 32; do
    for Q3_TEST_VALUE in -1 0 1 $((Q3_TEST_TEAM - 1)) "$Q3_TEST_TEAM" 2147483647 -2147483648; do
        "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/cgame" "$Q3_TEST_TEAM" "$Q3_TEST_VALUE"
    done
done
# Each is refused by orders and voiceOrders without formatting or sending
# anything; voiceOrdersTeam sends its string as it is. The formatted strings,
# the menus' seven voiceOrders among them, are the selected cases' above.
for Q3_TEST_ORDERS in 'cmd vtell %s offense' 'cmd vtell %n offense' 'cmd vtell %40000d offense' \
    'cmd vtell %d %d offense' 'cmd vtell %5d offense' 'cmd vtell %ld offense' 'cmd vtell %%d offense' \
    'cmd vtell %-d offense' 'cmd vtell %.1d offense' 'cmd vtell %d offense %' 'cmd vsay_team offense'; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" refused "$Q3_TEST_ORDERS"
done
# color1 is a game color, 1 to 7 (gamecodetoui has seven entries)
for Q3_TEST_VALUE in 0 1 4 7 8 -1 2147483647 -2147483648; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui" color1 "$Q3_TEST_VALUE"
done
# cg_drawCrosshair wraps at NUM_CROSSHAIRS (10) in the cgame
for Q3_TEST_VALUE in 0 3 9 10 12 -1 2147483647 -2147483648; do
    "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/q3ui" "$Q3_TEST_VALUE"
done
