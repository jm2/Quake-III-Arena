#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-module-print-bounds.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #450: the game, cgame and UI modules are linked natively, so the
# fixture takes each module's print and error functions verbatim from its real
# source (with a #line so reports name that file) and records the text they
# pass to the engine's print and error traps.
Q3_TEST_EXTRACT() {
    awk -v start="$1" -v file="$2" '
        index($0, start) == 1 { printf "#line %d \"%s\"\n", NR, file; found = 1 }
        found { print }
        found && /^}/ { done = 1; exit }
        END { exit !done }
    ' "$2" >> "$3" || {
        echo "run_module_print_bounds_tests: no $1 in $2" >&2
        exit 1
    }
}

Q3_TEST_GAME="$Q3_TEST_ROOT/code/game/g_main.c"
Q3_TEST_EXTRACT 'void QDECL G_Printf(' "$Q3_TEST_GAME" "$Q3_TEST_DIR/game.c"
Q3_TEST_EXTRACT 'void QDECL G_Error(' "$Q3_TEST_GAME" "$Q3_TEST_DIR/game.c"
Q3_TEST_EXTRACT 'void QDECL Com_Error (' "$Q3_TEST_GAME" "$Q3_TEST_DIR/game.c"
Q3_TEST_EXTRACT 'void QDECL Com_Printf(' "$Q3_TEST_GAME" "$Q3_TEST_DIR/game.c"

Q3_TEST_CGAME="$Q3_TEST_ROOT/code/cgame/cg_main.c"
Q3_TEST_EXTRACT 'void QDECL CG_Printf(' "$Q3_TEST_CGAME" "$Q3_TEST_DIR/cgame.c"
Q3_TEST_EXTRACT 'void QDECL CG_Error(' "$Q3_TEST_CGAME" "$Q3_TEST_DIR/cgame.c"
Q3_TEST_EXTRACT 'void QDECL Com_Error(' "$Q3_TEST_CGAME" "$Q3_TEST_DIR/cgame.c"
Q3_TEST_EXTRACT 'void QDECL Com_Printf(' "$Q3_TEST_CGAME" "$Q3_TEST_DIR/cgame.c"

for Q3_TEST_UI in q3_ui ui; do
    Q3_TEST_EXTRACT 'void QDECL Com_Error(' \
        "$Q3_TEST_ROOT/code/$Q3_TEST_UI/ui_atoms.c" "$Q3_TEST_DIR/$Q3_TEST_UI.c"
    Q3_TEST_EXTRACT 'void QDECL Com_Printf(' \
        "$Q3_TEST_ROOT/code/$Q3_TEST_UI/ui_atoms.c" "$Q3_TEST_DIR/$Q3_TEST_UI.c"
done

# Team Arena's menu parser messages; q_shared.c's are linked whole below.
Q3_TEST_EXTRACT 'void PC_SourceWarning(' "$Q3_TEST_ROOT/code/ui/ui_shared.c" "$Q3_TEST_DIR/shared.c"
Q3_TEST_EXTRACT 'void PC_SourceError(' "$Q3_TEST_ROOT/code/ui/ui_shared.c" "$Q3_TEST_DIR/shared.c"

Q3_TEST_BUILD() {
    local module="$1" define="$2"
    shift 2
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "-D$define" "-DQ3_TEST_MODULE=\"$module\"" \
        "-DQ3_TEST_BODIES=\"$Q3_TEST_DIR/$module.c\"" \
        "$Q3_TEST_ROOT/tests/module_print_bounds_regression.c" "$@" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$module"
}
Q3_TEST_SHARED_SOURCES=("$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c")
Q3_TEST_BUILD game Q3_TEST_GAME
Q3_TEST_BUILD cgame Q3_TEST_CGAME
Q3_TEST_BUILD q3_ui Q3_TEST_UI "${Q3_TEST_SHARED_SOURCES[@]}"
Q3_TEST_BUILD ui Q3_TEST_UI "${Q3_TEST_SHARED_SOURCES[@]}"
Q3_TEST_BUILD shared Q3_TEST_SHARED "${Q3_TEST_SHARED_SOURCES[@]}"

# One process per function: ASan stops at the first write past a buffer.
Q3_TEST_RUN() {
    local module="$1" function
    shift
    for function in "$@"; do
        # LeakSanitizer cannot initialize in the local ptrace sandbox.
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_DIR/$module" "$function"
    done
}
Q3_TEST_RUN game G_Printf G_Error Com_Printf Com_Error
Q3_TEST_RUN cgame CG_Printf CG_Error Com_Printf Com_Error
Q3_TEST_RUN q3_ui Com_Printf Com_Error
Q3_TEST_RUN ui Com_Printf Com_Error
Q3_TEST_RUN shared COM_ParseError COM_ParseWarning PC_SourceWarning PC_SourceError va
