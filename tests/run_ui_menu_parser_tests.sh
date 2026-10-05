#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-menu-parser.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #11: the Team Arena UI's UI_PC_* traps must reach botlib's precompiler,
# as in retail. One binary holds the engine side (the real cl_ui.c dispatcher,
# l_precomp.c and l_script.c) and the UI module side (the real ui_main.c and
# ui_shared.c, MISSIONPACK), and parses the retail-syntax corpus in
# tests/ui_menu_corpus with the repository's ui/menudef.h.
Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -fsanitize=address,undefined)
Q3_TEST_OBJECTS=()
for Q3_TEST_SOURCE in l_precomp l_script; do
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" -DBOTLIB -fgnu89-inline \
        -c "$Q3_TEST_ROOT/code/botlib/$Q3_TEST_SOURCE.c" -o "$Q3_TEST_DIR/$Q3_TEST_SOURCE.o"
    Q3_TEST_OBJECTS+=("$Q3_TEST_DIR/$Q3_TEST_SOURCE.o")
done
# The UI module's own Com_Printf and Com_Error count its parse errors.
for Q3_TEST_SOURCE in tests/ui_menu_parser_module code/ui/ui_shared; do
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" -DMISSIONPACK -DCom_Printf=UI_Com_Printf -DCom_Error=UI_Com_Error \
        -c "$Q3_TEST_ROOT/$Q3_TEST_SOURCE.c" -o "$Q3_TEST_DIR/$(basename "$Q3_TEST_SOURCE").o"
    Q3_TEST_OBJECTS+=("$Q3_TEST_DIR/$(basename "$Q3_TEST_SOURCE").o")
done
"${CC:-cc}" "${Q3_TEST_FLAGS[@]}" \
    "$Q3_TEST_ROOT/tests/ui_menu_parser_regression.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    "$Q3_TEST_ROOT/code/qcommon/vm.c" "${Q3_TEST_OBJECTS[@]}" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_menu_parser"

# LeakSanitizer cannot initialize in the local ptrace sandbox; the fixture
# counts its own allocations instead. The UI's load log goes to stdout.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/ui_menu_parser" "$Q3_TEST_ROOT/tests/ui_menu_corpus" "$Q3_TEST_ROOT" \
    > "$Q3_TEST_DIR/ui.log" || { cat "$Q3_TEST_DIR/ui.log"; exit 1; }
tail -n 1 "$Q3_TEST_DIR/ui.log"
