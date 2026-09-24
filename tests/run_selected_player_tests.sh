#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-selected-player.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# The Team Arena UI and cgame (MISSIONPACK) are linked natively. Both fixtures
# run the real CL_SystemInfoChanged and cvar.c behind the module's real syscall
# stubs, so a server's systeminfo sets the selected player cvars as it does in
# the client, and the modules read them back through trap_Cvar_Update and
# trap_Cvar_VariableValue.
Q3_TEST_ENGINE=(
    "$Q3_TEST_ROOT/tests/selected_player_systeminfo.c" "$Q3_TEST_ROOT/code/qcommon/cvar.c"
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c"
)
Q3_TEST_UI=()
for Q3_TEST_SOURCE in "$Q3_TEST_ROOT"/code/ui/ui_*.c; do
    # the fixture includes ui_main.c for its static menu script and item handlers
    if [[ "$Q3_TEST_SOURCE" != */ui_main.c ]]; then
        Q3_TEST_UI+=("$Q3_TEST_SOURCE")
    fi
done
# The UI reads cg_selectedPlayer back as a float; GCC's -fsanitize=undefined
# leaves out the float to int conversion check that Clang's includes.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined,float-cast-overflow -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/ui_selected_player_regression.c" "${Q3_TEST_UI[@]}" "${Q3_TEST_ENGINE[@]}" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui"
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/cgame_selected_player_regression.c" "$Q3_TEST_ROOT"/code/cgame/cg_*.c \
    "$Q3_TEST_ROOT/code/ui/ui_shared.c" "$Q3_TEST_ROOT/code/game/bg_misc.c" \
    "$Q3_TEST_ROOT/code/game/bg_pmove.c" "$Q3_TEST_ROOT/code/game/bg_slidemove.c" "${Q3_TEST_ENGINE[@]}" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/cgame"

# One process per selection. Each list's valid entries and its "Everyone"
# count must act as they always did; -1 and INT_MIN are below every list,
# INT_MAX is also past the int range once read back as a float (2^31), and a
# full 32-player team overlay's count is one past sortedTeamPlayers.
# LeakSanitizer cannot initialize in the local ptrace sandbox.
for Q3_TEST_SELECTED in -1 0 1 2 3 2147483647 -2147483648; do
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/ui" "$Q3_TEST_SELECTED"
done
for Q3_TEST_TEAM in 3 32; do
    for Q3_TEST_SELECTED in -1 0 1 $((Q3_TEST_TEAM - 1)) "$Q3_TEST_TEAM" 2147483647 -2147483648; do
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_DIR/cgame" "$Q3_TEST_TEAM" "$Q3_TEST_SELECTED"
    done
done
