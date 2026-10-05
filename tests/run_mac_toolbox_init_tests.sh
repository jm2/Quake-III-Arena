#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-mac-toolbox-init.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issues #263 and #10: main initializes the Toolbox before anything else, and
# the Retro68 console window opens only when output is wanted.  mac_main.c and
# mac_console.c need the whole Mac Toolbox, so the fixture takes main and the
# functions it runs from them verbatim (with a #line so reports name those
# files) and fakes the Toolbox calls and the console hooks around them.
Q3_TEST_EXTRACT() {
    awk -v start="$1" -v file="$2" '
        $0 ~ start { printf "#line %d \"%s\"\n", NR, file; found = 1 }
        found { print }
        found && /^}/ { found = 0; count++ }
        END { exit count == 1 ? 0 : 1 }
    ' "$2" >> "$3" || { echo "run_mac_toolbox_init_tests: no single $1 in $2" >&2; exit 1; }
}
Q3_TEST_MAIN="$Q3_TEST_ROOT/code/mac/mac_main.c"
Q3_TEST_CONSOLE="$Q3_TEST_ROOT/code/mac/mac_console.c"
awk '/^#define[[:space:]]+MAC_(PARMS_FILE|COMMAND_LINE_SIZE)[[:space:]]/' "$Q3_TEST_MAIN" \
    > "$Q3_TEST_DIR/mac_main_extracted.c"
[ "$(wc -l < "$Q3_TEST_DIR/mac_main_extracted.c")" -eq 2 ] ||
    { echo "run_mac_toolbox_init_tests: no MAC_PARMS_FILE/MAC_COMMAND_LINE_SIZE in $Q3_TEST_MAIN" >&2; exit 1; }
Q3_TEST_EXTRACT '^static void[[:space:]]+Sys_InitToolbox[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_DIR/mac_main_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+Sys_LogPrintf[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_DIR/mac_main_extracted.c"
Q3_TEST_EXTRACT '^static int[[:space:]]+Sys_AppendStartupText[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_DIR/mac_main_extracted.c"
Q3_TEST_EXTRACT '^static int[[:space:]]+Sys_ReadStartupFile[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_DIR/mac_main_extracted.c"
Q3_TEST_EXTRACT '^static int[[:space:]]+Sys_StartupError[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_DIR/mac_main_extracted.c"
Q3_TEST_EXTRACT '^int[[:space:]]+main[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_DIR/mac_main_extracted.c"
Q3_TEST_EXTRACT '^int[[:space:]]+Sys_ConsoleWanted[[:space:]]*[(]' "$Q3_TEST_CONSOLE" "$Q3_TEST_DIR/mac_console_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+Sys_ShowConsole[[:space:]]*[(]' "$Q3_TEST_CONSOLE" "$Q3_TEST_DIR/mac_console_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+Sys_Print[[:space:]]*[(]' "$Q3_TEST_CONSOLE" "$Q3_TEST_DIR/mac_console_extracted.c"

"${CC:-cc}" \
    -std=gnu99 -fsigned-char -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/mac_toolbox_init_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/mac_toolbox_init"

ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/mac_toolbox_init" "$Q3_TEST_DIR"
