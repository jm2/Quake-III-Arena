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
# Issue #459: then builds the Team Arena cgame and ui the same way, each with
# its own code/ui/ui_shared.c (the cgame's renamed by ui_shared_cgame.h), and
# runs tests/static_module_reset_ta_regression.c. After each link the runner
# checks what cmake/static_modules.py checks on the Mac: every writable
# section of a module lies in its own brackets, no global is defined by two
# modules, and no module uses another module's symbols.
#
# The modules talk to a stub engine. The real *_syscalls.c pass pointers
# through int varargs, so the runner generates a weak default for every trap
# of each module's *_syscalls.c instead, and the test overrides the traps the
# scenarios need.
#
# ASan puts redzones between instrumented globals, and the reset copies and
# zeroes whole brackets, so module objects are built without ASan's global
# instrumentation; stack, heap and UBSan checks stay on, including the array
# bounds check since #463 bounded CalculateRanks' team voting loop, except
# clang's null member access check, which this test is not about (the retail
# FOFS() is &((gentity_t *)0)->field).
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

q3_mkdirs() {
    local work="$1" d
    shift
    for d in "$@"; do
        mkdir -p "$work/$d"
    done
}
q3_mkdirs "$Q3_TEST_DIR" m-game m-cgame m-ui shared engine
q3_mkdirs "$Q3_TEST_DIR/ta" m-cgame m-ui shared engine
q3_trap_stubs "$C/game/g_syscalls.c" "$C/game/g_local.h" "$Q3_TEST_DIR/game_traps.c"
q3_trap_stubs "$C/cgame/cg_syscalls.c" "$C/cgame/cg_local.h" "$Q3_TEST_DIR/cgame_traps.c"
q3_trap_stubs "$C/q3_ui/ui_syscalls.c" "$C/q3_ui/ui_local.h" "$Q3_TEST_DIR/ui_traps.c"
q3_trap_stubs "$C/ui/ui_syscalls.c" "$C/ui/ui_local.h" "$Q3_TEST_DIR/ta/ui_traps.c"
Q3_TA=(-DMISSIONPACK)

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

    # Team Arena: the cgame with cg_newdraw.c, -DCGAME and its own
    # ui_shared.c; the ui with code/ui, ui_shared.c included.
    for b in bg_misc bg_pmove bg_slidemove q_math q_shared; do
        echo "ta/shared/$b.o $C/game/$b.c module ${Q3_GAME[*]} ${Q3_TA[*]}"
    done
    for f in "$C"/cgame/*.c "$C"/ui/ui_shared.c; do
        b="$(basename "$f" .c)"
        case "$b" in
            cg_syscalls) ;;
            *) echo "ta/m-cgame/$b.o $f module ${Q3_CGAME[*]} -DCGAME ${Q3_TA[*]}";;
        esac
    done
    for f in "$C"/ui/*.c; do
        b="$(basename "$f" .c)"
        case "$b" in
            ui_syscalls) ;;
            *) echo "ta/m-ui/$b.o $f module ${Q3_UI[*]} ${Q3_TA[*]}";;
        esac
    done
    echo "ta/engine/cgame_traps.o $Q3_TEST_DIR/cgame_traps.c module ${Q3_CGAME[*]} -DCGAME ${Q3_TA[*]}"
    echo "ta/engine/ui_traps.o $Q3_TEST_DIR/ta/ui_traps.c module ${Q3_UI[*]} ${Q3_TA[*]}"
    echo "ta/engine/vm_static.o $C/qcommon/vm_static.c test"
    T="$Q3_TEST_ROOT/tests/static_module_reset_ta_regression.c"
    echo "ta/engine/test_cgame.o $T test ${Q3_CGAME[*]} -DCGAME ${Q3_TA[*]} -DQ3_TEST_CGAME"
    echo "ta/engine/test_ui.o $T test ${Q3_UI[*]} ${Q3_TA[*]} -DQ3_TEST_UI"
    echo "ta/engine/test_main.o $T test ${Q3_TA[*]}"
} > "$Q3_TEST_DIR/objects.txt"

# Module code is retail code: its warnings are not this test's business.
q3_compile() {
    local out="$1" src="$2" kind="$3"
    shift 3
    if [ "$kind" = module ]; then
        "$Q3_CC" "${Q3_FLAGS[@]}" "${Q3_NO_ASAN_GLOBALS[@]}" -fno-sanitize=null -w "$@" \
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

# q3_link <dir> <program> <module>...: links <dir>'s objects into <program>
# with each module's data and bss, bracketed, after the program's own. 32-bit
# PowerPC ELF puts small globals in .sdata and .sbss. The check after the link
# makes sure no other writable section of a module escapes the brackets.
q3_link() {
    local work="$1" program="$2" module
    shift 2
    local data='.data .data.* .sdata .sdata.*' bss='.bss .bss.* .sbss .sbss.* COMMON'
    {
        echo 'SECTIONS'
        echo '{'
        echo '  .q3static.data : {'
        for module in "$@"; do
            echo "    . = ALIGN(16); q3static_${module}_data_start = .;"
            echo "    */m-$module/*.o($data)"
            echo "    . = ALIGN(16); q3static_${module}_data_end = .;"
        done
        echo '  }'
        echo '}'
        echo 'INSERT AFTER .data;'
        echo 'SECTIONS'
        echo '{'
        echo '  .q3static.bss (NOLOAD) : {'
        for module in "$@"; do
            echo "    . = ALIGN(16); q3static_${module}_bss_start = .;"
            echo "    */m-$module/*.o($bss)"
            echo "    . = ALIGN(16); q3static_${module}_bss_end = .;"
        done
        echo '  }'
        echo '}'
        echo 'INSERT AFTER .bss;'
    } > "$work/brackets.ld"
    local objects=("$work"/engine/*.o "$work"/shared/*.o)
    for module in "$@"; do
        objects+=("$work/m-$module"/*.o)
    done
    "$Q3_CC" -fsanitize=address,undefined -no-pie "${objects[@]}" \
        -Wl,-T,"$work/brackets.ld" -Wl,-Map,"$work/$program.map" -lm -o "$work/$program"
    q3_check_brackets "$work" "$program" "$@"
}

# What cmake/static_modules.py check does for the Mac link: every allocated,
# writable section of a module object must lie in that module's brackets, and
# nothing else may lie in one; no global symbol may be defined by two modules,
# and no module may use a symbol another module defines. Startup tables and
# GOTs hold no module state. Prints which objects more than one module links
# its own copy of (ui_shared.o in Team Arena, #459) and where each copy lies.
q3_check_brackets() {
    python3 - "${READELF:-readelf}" "${NM:-nm}" "$@" <<'EOF'
import glob, os, re, subprocess, sys
readelf, nm, work, program, modules = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5:]
stateless = re.compile(r'^\.((preinit_array|init_array|fini_array|ctors|dtors)(\..*)?'
                       r'|got2?|eh_frame|tm_clone_table)$')
lines = open(os.path.join(work, program + '.map')).read().split('\n')
start = next(i for i, l in enumerate(lines) if l.startswith('Linker script and memory map'))
symbols, placed, pending = {}, [], None
for line in lines[start:]:
    m = re.match(r'^\s+0x([0-9a-f]+)\s+(q3static_\w+) = ', line)
    if m:
        symbols[m.group(2)] = int(m.group(1), 16)
        continue
    m = re.match(r'^ (\.\S+|COMMON)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+) (\S+)$', line)
    if m:
        placed.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4)))
        pending = None
        continue
    m = re.match(r'^ (\.\S+|COMMON)$', line)
    if m:
        pending = m.group(1)
        continue
    m = re.match(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+) (\S+)$', line) if pending else None
    if m:
        placed.append((pending, int(m.group(1), 16), int(m.group(2), 16), m.group(3)))
    pending = None
brackets = {}
for module in modules:
    brackets[module] = [(symbols['q3static_%s_%s_start' % (module, kind)],
                         symbols['q3static_%s_%s_end' % (module, kind)]) for kind in ('data', 'bss')]
section = re.compile(r'^\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)\s+'
                     r'[0-9a-f]+\s+([A-Za-z]*)\s+\d+\s+\d+\s+\d+\s*$', re.M)
errors, checked, copies = [], 0, {}
defined, used = {}, {}
for module in brackets:
    for obj in sorted(glob.glob(os.path.join(work, 'm-' + module, '*.o'))):
        out = subprocess.run([readelf, '-S', '-W', obj], check=True, stdout=subprocess.PIPE).stdout.decode()
        for m in section.finditer(out):
            name, size, flags = m.group(1), int(m.group(2), 16), m.group(3)
            if not size or 'A' not in flags or 'W' not in flags or stateless.match(name):
                continue
            checked += 1
            where = [(a, n) for sec, a, n, f in placed
                     if sec == name and os.path.realpath(f) == os.path.realpath(obj)]
            if not where or not all(any(lo <= a and a + n <= hi for lo, hi in brackets[module])
                                    for a, n in where):
                errors.append('%s %s (%d bytes) is outside the %s brackets'
                              % (os.path.relpath(obj, work), name, size, module))
            copies.setdefault(os.path.basename(obj), {}).setdefault(module, []).extend(where)
        out = subprocess.run([nm, obj], check=True, stdout=subprocess.PIPE).stdout.decode()
        for m in re.finditer(r'^\s*[0-9a-f]*\s+([A-Za-z])\s+(\S+)$', out, re.M):
            kind, name = m.group(1), m.group(2)
            if kind == 'U':
                used.setdefault(name, set()).add(module)
            elif kind.isupper() and kind not in 'NW':
                defined.setdefault(name, set()).add(module)
for sec, a, n, f in placed:
    if n and '/m-' not in f:
        for module, ranges in brackets.items():
            if any(a < hi and lo < a + n for lo, hi in ranges):
                errors.append('%s %s lies inside the %s brackets' % (f, sec, module))
for name, where in sorted(defined.items()):
    if len(where) > 1:
        errors.append('%s is defined in more than one module: %s' % (name, ', '.join(sorted(where))))
    for user in sorted(used.get(name, set()) - where):
        errors.append('the %s module uses %s from the %s module' % (user, name, ', '.join(sorted(where))))
if errors or not checked:
    sys.exit('%s: static module brackets:\n  ' % program + '\n  '.join(errors or ['no module section was checked']))
print('%s: static module brackets: %d writable module sections checked' % (program, checked))
for obj, per in sorted(copies.items()):
    if len(per) > 1:
        print('%s: one copy of %s per module: %s' % (program, obj, '; '.join(
            '%s %s' % (module, ', '.join('0x%x+%d' % w for w in sorted(per[module])))
            for module in modules if module in per)))
EOF
}

q3_link "$Q3_TEST_DIR" static_module_reset game cgame ui
q3_link "$Q3_TEST_DIR/ta" static_module_reset_ta cgame ui

# LeakSanitizer cannot initialize in the local ptrace sandbox.
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$Q3_TEST_DIR/static_module_reset"
"$Q3_TEST_DIR/ta/static_module_reset_ta"
