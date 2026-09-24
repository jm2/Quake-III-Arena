#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-file-list.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# The Team Arena UI (code/ui, MISSIONPACK) and the base q3_ui menus are linked
# natively, so the fixtures include the real ui_main.c (movie and demo lists)
# and ui_loadconfig.c and link ui_demo2.c (config and demo lists), and serve
# them file lists that hold names shorter than, as long as and longer than the
# extension each list strips.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/ui_file_list_regression.c" \
    "$Q3_TEST_ROOT/code/ui/ui_shared.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_file_list"
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/q3ui_file_list_regression.c" \
    "$Q3_TEST_ROOT/code/q3_ui/ui_demo2.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/q3ui_file_list"

for Q3_TEST_BINARY in ui_file_list q3ui_file_list; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_BINARY"
done
