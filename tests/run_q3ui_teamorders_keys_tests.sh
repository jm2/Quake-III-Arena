#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-q3ui-teamorders-keys.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# The base q3_ui menus are linked natively, so the fixture includes the real
# ui_teamorders.c and links the real menu framework (ui_qmenu.c) and menu
# stack, mouse and key routing (ui_atoms.c).
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/q3ui_teamorders_keys_regression.c" \
    "$Q3_TEST_ROOT/code/q3_ui/ui_qmenu.c" "$Q3_TEST_ROOT/code/q3_ui/ui_atoms.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/q3ui_teamorders_keys"

# One process per input and game type: mouse clicks on every pixel row of the
# full bot list and of the CTF and team orders, and the keyboard.
for Q3_TEST_GAMETYPE in ctf team; do
    for Q3_TEST_INPUT in bots orders keys; do
        # LeakSanitizer cannot initialize in the local ptrace sandbox.
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_DIR/q3ui_teamorders_keys" "$Q3_TEST_INPUT" "$Q3_TEST_GAMETYPE"
    done
done
