#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-menu-alloc.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #49: Team Arena menus (code/ui/ui_shared.c, MISSIONPACK) parsed with
# every budget the UI memory and string pools can have, built as the ui module
# and with -DCGAME, whose pools are smaller; and the ui module's menu reload
# (UI_Load) and player model cvars (ui_main.c, ui_players.c).
for Q3_TEST_MODULE in ui cgame; do
    Q3_TEST_DEFINES=-DMISSIONPACK
    if [ "$Q3_TEST_MODULE" = cgame ]; then
        Q3_TEST_DEFINES="$Q3_TEST_DEFINES -DCGAME"
    fi
    # shellcheck disable=SC2086
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined $Q3_TEST_DEFINES \
        "$Q3_TEST_ROOT/tests/ui_menu_alloc_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_menu_alloc_$Q3_TEST_MODULE"
done
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/ui_reload_model_regression.c" \
    "$Q3_TEST_ROOT/code/ui/ui_shared.c" "$Q3_TEST_ROOT/code/ui/ui_players.c" \
    "$Q3_TEST_ROOT/code/game/bg_misc.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_reload_model"

# One process per case. LeakSanitizer cannot initialize in the local ptrace
# sandbox. Frames stay on the real stack, so the reload case can leave a stale
# menu name where UI_Load's name buffer will be.
run() {
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:detect_stack_use_after_return=0 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$1" "$2"
}
for Q3_TEST_MODULE in ui cgame; do
    for Q3_TEST_CASE in node memory strings set; do
        run "ui_menu_alloc_$Q3_TEST_MODULE" "$Q3_TEST_CASE"
    done
done
for Q3_TEST_CASE in reload-focused reload-unfocused cvar63 cvar64 cvar255; do
    run ui_reload_model "$Q3_TEST_CASE"
done
