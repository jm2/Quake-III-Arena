#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-q3ui-name-bounds.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Found auditing issue #440: the base q3_ui is linked natively, so each fixture
# includes the real menu source and serves it names from pk3 entries and .arena
# files, mod directories or arenas that do not fit the menu's fixed buffers or
# lists: the Player Model menu (ui_playermodel.c), the single player levelshots
# (ui_splevel.c), the Mods menu (ui_mods.c) and the Create and Skirmish map
# lists (ui_startserver.c).
for Q3_TEST_NAME in playermodel_name splevel_levelshot mods_list startserver_maps; do
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined \
        "$Q3_TEST_ROOT/tests/q3ui_${Q3_TEST_NAME}_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/q3ui_$Q3_TEST_NAME"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/q3ui_$Q3_TEST_NAME"
done
