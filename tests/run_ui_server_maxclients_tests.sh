#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-maxclients.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# The Team Arena UI (code/ui, MISSIONPACK) and the base q3_ui menus are linked
# natively, so the fixtures include the real ui_main.c, ui_removebots.c and
# ui_teamorders.c and serve them a hostile server's config strings.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/ui_server_maxclients_regression.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_maxclients"
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/q3ui_server_maxclients_regression.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/q3ui_maxclients"

# One process per server layout and sv_maxclients value. A sparse server has
# empty and red slots, a full one has every slot taken by the local client's
# team (or by bots), and both fill every string past the player slots.
# 8, 16 and 64 are real servers and must list what they always did; 65, 1024
# and INT_MAX would read past CS_PLAYERS + MAX_CLIENTS (and, past
# MAX_CONFIGSTRINGS, keep the last string) and write past the player lists;
# negative values list nobody.
for Q3_TEST_LAYOUT in sparse full; do
    for Q3_TEST_MAXCLIENTS in 8 16 64 65 1024 2147483647 -1 -2147483648; do
        for Q3_TEST_CASE in "ui_maxclients" "q3ui_maxclients removebots" "q3ui_maxclients teamorders"; do
            # LeakSanitizer cannot initialize in the local ptrace sandbox.
            # shellcheck disable=SC2086 # the case is a binary and its menu
            ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
            UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
                "$Q3_TEST_DIR"/$Q3_TEST_CASE "$Q3_TEST_LAYOUT" "$Q3_TEST_MAXCLIENTS"
        done
    done
done
