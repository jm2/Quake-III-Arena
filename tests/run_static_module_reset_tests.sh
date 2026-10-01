#!/usr/bin/env bash
# Issue #457: Sys_LoadDll restores each statically linked module's fresh image
# (code/qcommon/vm_static.c) before every load, as a retail QVM load did.
#
# Builds the real game, cgame and q3_ui sources as CMakeLists.txt does for the
# static build (Q3_STATIC, the module's vmMain/dllEntry names, bg_*.c,
# q_shared.c and q_math.c once, outside the modules), links them into one
# program with each module's .data and .bss bracketed by
# q3static_<module>_{data,bss}_{start,end} (a GNU ld script; on the Mac,
# cmake/static_modules.py writes the XCOFF equivalent), and runs
# tests/static_module_reset_regression.c.
#
# The modules talk to a stub engine. The real *_syscalls.c pass pointers
# through int varargs, so the runner generates a weak default for every trap
# of each module's *_syscalls.c instead, and the test overrides the traps the
# scenarios need.
#
# ASan puts redzones between instrumented globals, and the reset copies and
# zeroes whole brackets, so module objects are built without ASan's global
# instrumentation; stack, heap and UBSan checks stay on, except two that trip
# on retail idioms this test is not about: the array bounds check (retail
# CalculateRanks clears numteamVotingClients[0..3] of an int[2] inside
# level_locals_t) and clang's null member access check (FOFS() is
# &((gentity_t *)0)->field).
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-static-module-reset.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
Q3_CC="${CC:-cc}"
Q3_JOBS="${Q3_TEST_JOBS:-4}"
C="$Q3_TEST_ROOT/code"

if "$Q3_CC" --version 2>/dev/null | grep -qi clang; then
    Q3_NO_ASAN_GLOBALS=(-mllvm -asan-globals=0)
else
    Q3_NO_ASAN_GLOBALS=(--param asan-globals=0)
fi
Q3_FLAGS=(-std=gnu99 -fgnu89-inline -fno-strict-aliasing -fsigned-char -fno-common
    -fno-omit-frame-pointer -fno-pie -fsanitize=address,undefined -DQ3_STATIC -I"$C/qcommon")
Q3_GAME=(-DGAME_MODULE -DvmMain=Game_vmMain -DdllEntry=Game_dllEntry)
Q3_CGAME=(-DCGAME_MODULE -DvmMain=CGame_vmMain -DdllEntry=CGame_dllEntry)
Q3_UI=(-DUI_MODULE -DvmMain=UI_vmMain -DdllEntry=UI_dllEntry)

# A weak default for every trap_* definition in a *_syscalls.c.
q3_trap_stubs() {
    python3 - "$1" "$2" > "$3" <<'EOF'
import re, sys
text = open(sys.argv[1], encoding='latin-1').read()
text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
text = re.sub(r'//[^\n]*', '', text)
print('#include "%s"' % sys.argv[2])
print('void Q3T_DefaultTrap( const char *name );')
seen = set()
for m in re.finditer(r'^([A-Za-z_][\w \t*]*?)\b(trap_\w+)\s*\(([^)]*)\)\s*\{', text, re.M):
    rtype, name, params = m.group(1).strip(), m.group(2), ' '.join(m.group(3).split())
    if name in seen:
        continue
    seen.add(name)
    ret = '' if rtype == 'void' else ' return (%s)0;' % rtype
    print('__attribute__((weak)) %s %s( %s ) { Q3T_DefaultTrap( "%s" );%s }' % (rtype, name, params or 'void', name, ret))
if len(seen) < 50:
    sys.exit('only %d traps found in %s' % (len(seen), sys.argv[1]))
EOF
}

mkdir -p "$Q3_TEST_DIR/m-game" "$Q3_TEST_DIR/m-cgame" "$Q3_TEST_DIR/m-ui" "$Q3_TEST_DIR/shared" "$Q3_TEST_DIR/engine"
q3_trap_stubs "$C/game/g_syscalls.c" "$C/game/g_local.h" "$Q3_TEST_DIR/game_traps.c"
q3_trap_stubs "$C/cgame/cg_syscalls.c" "$C/cgame/cg_local.h" "$Q3_TEST_DIR/cgame_traps.c"
q3_trap_stubs "$C/q3_ui/ui_syscalls.c" "$C/q3_ui/ui_local.h" "$Q3_TEST_DIR/ui_traps.c"

# "output source flags..." per line, the sources as CMakeLists.txt groups them.
{
    for f in "$C"/game/*.c; do
        b="$(basename "$f" .c)"
        case "$b" in
            g_rankings|bg_lib|g_syscalls) ;;
            bg_misc|bg_pmove|bg_slidemove|q_math|q_shared) echo "shared/$b.o $f module ${Q3_GAME[*]}";;
            *) echo "m-game/$b.o $f module ${Q3_GAME[*]}";;
        esac
    done
    for f in "$C"/cgame/*.c; do
        b="$(basename "$f" .c)"
        case "$b" in
            cg_syscalls|cg_newdraw) ;;
            *) echo "m-cgame/$b.o $f module ${Q3_CGAME[*]}";;
        esac
    done
    for f in "$C"/q3_ui/*.c; do
        b="$(basename "$f" .c)"
        case "$b" in
            ui_syscalls|ui_rankings|ui_rankstatus|ui_signup|ui_login|ui_specifyleague) ;;
            *) echo "m-ui/$b.o $f module ${Q3_UI[*]}";;
        esac
    done
    echo "engine/game_traps.o $Q3_TEST_DIR/game_traps.c module ${Q3_GAME[*]}"
    echo "engine/cgame_traps.o $Q3_TEST_DIR/cgame_traps.c module ${Q3_CGAME[*]}"
    echo "engine/ui_traps.o $Q3_TEST_DIR/ui_traps.c module ${Q3_UI[*]}"
    echo "engine/vm_static.o $C/qcommon/vm_static.c test"
    T="$Q3_TEST_ROOT/tests/static_module_reset_regression.c"
    echo "engine/test_game.o $T test ${Q3_GAME[*]} -DQ3_TEST_GAME"
    echo "engine/test_cgame.o $T test ${Q3_CGAME[*]} -DQ3_TEST_CGAME"
    echo "engine/test_ui.o $T test ${Q3_UI[*]} -DQ3_TEST_UI"
    echo "engine/test_main.o $T test"
} > "$Q3_TEST_DIR/objects.txt"

# Module code is retail code: its warnings are not this test's business.
q3_compile() {
    local out="$1" src="$2" kind="$3"
    shift 3
    if [ "$kind" = module ]; then
        "$Q3_CC" "${Q3_FLAGS[@]}" "${Q3_NO_ASAN_GLOBALS[@]}" -fno-sanitize=bounds,null -w "$@" \
            -c "$src" -o "$Q3_TEST_DIR/$out"
    else
        "$Q3_CC" "${Q3_FLAGS[@]}" -Wall "$@" -c "$src" -o "$Q3_TEST_DIR/$out"
    fi
}
export -f q3_compile
export Q3_CC Q3_TEST_DIR
export Q3_FLAGS_STR="${Q3_FLAGS[*]}" Q3_NO_ASAN_GLOBALS_STR="${Q3_NO_ASAN_GLOBALS[*]}"
# shellcheck disable=SC2016
xargs -P "$Q3_JOBS" -L 1 bash -c '
    Q3_FLAGS=($Q3_FLAGS_STR); Q3_NO_ASAN_GLOBALS=($Q3_NO_ASAN_GLOBALS_STR)
    q3_compile "$@"' _ < "$Q3_TEST_DIR/objects.txt"

# Each module's data and bss, bracketed, after the program's own.
{
    echo 'SECTIONS'
    echo '{'
    echo '  .q3static.data : {'
    for module in game cgame ui; do
        echo "    . = ALIGN(16); q3static_${module}_data_start = .;"
        echo "    */m-$module/*.o(.data .data.*)"
        echo "    . = ALIGN(16); q3static_${module}_data_end = .;"
    done
    echo '  }'
    echo '}'
    echo 'INSERT AFTER .data;'
    echo 'SECTIONS'
    echo '{'
    echo '  .q3static.bss (NOLOAD) : {'
    for module in game cgame ui; do
        echo "    . = ALIGN(16); q3static_${module}_bss_start = .;"
        echo "    */m-$module/*.o(.bss .bss.* COMMON)"
        echo "    . = ALIGN(16); q3static_${module}_bss_end = .;"
    done
    echo '  }'
    echo '}'
    echo 'INSERT AFTER .bss;'
} > "$Q3_TEST_DIR/brackets.ld"

"$Q3_CC" -fsanitize=address,undefined -no-pie \
    "$Q3_TEST_DIR"/engine/*.o "$Q3_TEST_DIR"/shared/*.o \
    "$Q3_TEST_DIR"/m-game/*.o "$Q3_TEST_DIR"/m-cgame/*.o "$Q3_TEST_DIR"/m-ui/*.o \
    -Wl,-T,"$Q3_TEST_DIR/brackets.ld" -lm -o "$Q3_TEST_DIR/static_module_reset"

# LeakSanitizer cannot initialize in the local ptrace sandbox.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/static_module_reset"
