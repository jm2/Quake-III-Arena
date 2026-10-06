#!/usr/bin/env bash
# Issue #395: the printf-style functions of the engine and the modules carry
# Q_PRINTF_FORMAT (code/game/q_shared.h), and the build fails on a format
# whose arguments do not match. This compile-only runner checks that:
#   1. every call in tests/format_attribute_regression.c builds with the
#      build's format flags, and a planted mismatch in any one of them fails
#      with a format error on that line, so no function loses its attribute;
#   2. Q_PRINTF_FORMAT expands to nothing for QVM builds (q3lcc has no
#      __attribute__);
#   3. CMakeLists.txt (the Retro68 build) and tests/format_cc.sh (every
#      runner under run_host_regressions.sh) still use these flags;
#   4. every engine and module source this host can compile (all but
#      code/mac, which the Retro68 build covers) builds with those flags, for
#      each module configuration CMakeLists.txt builds.
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
export LC_ALL=C
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-format-attribute.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
Q3_TEST_SOURCE="$Q3_TEST_ROOT/tests/format_attribute_regression.c"

# The flags CMakeLists.txt adds for the Retro68 build.
Q3_FORMAT_FLAGS=(-Wformat=2 -Wno-format-nonliteral -Werror=format -Werror=format-security -Wno-error=format-overflow -Wno-error=format-truncation)
Q3_FLAGS=(-fsyntax-only -std=gnu99 -fgnu89-inline -fsigned-char -DBOTLIB -DQ3_STATIC
    "${Q3_FORMAT_FLAGS[@]}" -I"$Q3_TEST_ROOT/code")
for Q3_DIR in game cgame ui client server renderer qcommon botlib; do
    Q3_FLAGS+=(-I"$Q3_TEST_ROOT/code/$Q3_DIR")
done

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

# 1. Planted mismatches. Each Q3_GOOD call line belongs to the section of the
#    last "#ifdef Q3_FORMAT_TU_" above it.
mapfile -t Q3_PLANTS < <(awk '/^#ifdef Q3_FORMAT_TU_/ { tu = $2 }
    tu != "" && /Q3_GOOD/ { print NR " " tu }' "$Q3_TEST_SOURCE")
[ "${#Q3_PLANTS[@]}" -ge 30 ] || fail "found only ${#Q3_PLANTS[@]} checked calls in $Q3_TEST_SOURCE"
for Q3_TU in $(printf '%s\n' "${Q3_PLANTS[@]}" | awk '{ print $2 }' | sort -u); do
    "${CC:-cc}" "${Q3_FLAGS[@]}" -D"$Q3_TU" "$Q3_TEST_SOURCE" ||
        fail "the correct calls of $Q3_TU do not build"
done
for Q3_PLANT in "${Q3_PLANTS[@]}"; do
    read -r Q3_LINE Q3_TU <<< "$Q3_PLANT"
    Q3_COPY="$Q3_TEST_DIR/format_attribute_regression.c"
    sed "${Q3_LINE}s/Q3_GOOD/Q3_BAD/" "$Q3_TEST_SOURCE" > "$Q3_COPY"
    Q3_CALL="$(sed -n "${Q3_LINE}p" "$Q3_TEST_SOURCE" | sed 's/^[[:space:]]*//')"
    if "${CC:-cc}" "${Q3_FLAGS[@]}" -D"$Q3_TU" "$Q3_COPY" > "$Q3_TEST_DIR/plant.log" 2>&1; then
        fail "a mismatched format still builds (no format attribute?): $Q3_CALL"
    fi
    # GCC tags the error [-Werror=format=], Clang [-Werror,-Wformat].
    grep -Eq "format_attribute_regression\.c:$Q3_LINE:.*error:.*-Werror[=,](-W)?format" "$Q3_TEST_DIR/plant.log" || {
        cat "$Q3_TEST_DIR/plant.log" >&2
        fail "the planted mismatch did not fail as a format error on line $Q3_LINE: $Q3_CALL"
    }
done
echo "PASS: ${#Q3_PLANTS[@]} printf-style calls are format-checked"

# 2. QVM builds: q3lcc defines Q3_VM and does not know __attribute__.
"${CC:-cc}" -E -P -DQ3_VM -I"$Q3_TEST_ROOT/code/game" "$Q3_TEST_ROOT/code/game/q_shared.h" > "$Q3_TEST_DIR/vm.i"
grep -q 'Com_Printf' "$Q3_TEST_DIR/vm.i" || fail "the Q3_VM preprocessing lost Com_Printf"
! grep -q '__attribute__' "$Q3_TEST_DIR/vm.i" || fail "q_shared.h uses __attribute__ in a Q3_VM build"
"${CC:-cc}" -E -P -I"$Q3_TEST_ROOT/code/game" "$Q3_TEST_ROOT/code/game/q_shared.h" > "$Q3_TEST_DIR/native.i"
grep -Eq 'Com_Printf\(.*__attribute__ ?\(\(format ?\(printf, 1, 2\)\)\)' "$Q3_TEST_DIR/native.i" ||
    fail "Com_Printf has no format attribute in a native build"
echo "PASS: Q_PRINTF_FORMAT is empty for Q3_VM"

# 3. The Retro68 build.
grep -Fq "set(CMAKE_C_FLAGS \"\${CMAKE_C_FLAGS} ${Q3_FORMAT_FLAGS[*]}\")" "$Q3_TEST_ROOT/CMakeLists.txt" ||
    fail "CMakeLists.txt no longer builds with ${Q3_FORMAT_FLAGS[*]}"
grep -Fq -- "${Q3_FORMAT_FLAGS[*]:0:4}" "$Q3_TEST_ROOT/tests/format_cc.sh" ||
    fail "tests/format_cc.sh no longer adds ${Q3_FORMAT_FLAGS[*]:0:4}"
echo "PASS: CMakeLists.txt and tests/format_cc.sh use these format checks"

# 4. The tree. code/renderer includes <GL/gl.h>; it needs only the GL types
#    and constant names (as in run_duplicate_global_tests.sh).
mkdir -p "$Q3_TEST_DIR/gl/GL"
{
    echo 'typedef unsigned int GLenum, GLbitfield, GLuint; typedef int GLint, GLsizei;'
    echo 'typedef unsigned char GLboolean, GLubyte; typedef signed char GLbyte; typedef void GLvoid;'
    echo 'typedef short GLshort; typedef unsigned short GLushort;'
    echo 'typedef float GLfloat, GLclampf; typedef double GLdouble, GLclampd;'
    comm -23 \
        <(grep -ohE '\bGL_[A-Z0-9_]+\b' "$Q3_TEST_ROOT"/code/renderer/*.[ch] | sort -u) \
        <(grep -ohE '^[[:space:]]*#[[:space:]]*define[[:space:]]+GL_[A-Z0-9_]+\b' \
            "$Q3_TEST_ROOT"/code/renderer/*.[ch] | awk '{ print $NF }' | sort -u) |
        awk '{ printf "#define %s %d\n", $1, NR }'
} > "$Q3_TEST_DIR/gl/GL/gl.h"
printf '%s\n' 'typedef struct _XDisplay Display; typedef struct XVisualInfo XVisualInfo;' \
    'typedef struct __GLXcontextRec *GLXContext; typedef unsigned long GLXDrawable; typedef int Bool;' \
    > "$Q3_TEST_DIR/gl/GL/glx.h"
Q3_FLAGS+=(-I"$Q3_TEST_DIR/gl")
Q3_SWEPT=0
sweep() {
    local file="$1"
    shift
    "${CC:-cc}" "${Q3_FLAGS[@]}" "$@" "$file" > "$Q3_TEST_DIR/sweep.log" 2>&1 || {
        cat "$Q3_TEST_DIR/sweep.log" >&2
        fail "${file#"$Q3_TEST_ROOT"/} ($*) does not build with ${Q3_FORMAT_FLAGS[*]}"
    }
    Q3_SWEPT=$((Q3_SWEPT + 1))
}
# The source lists and exclusions of CMakeLists.txt.
for Q3_FILE in "$Q3_TEST_ROOT"/code/{qcommon,server,client,renderer,botlib}/*.c; do
    case "$(basename "$Q3_FILE")" in
        vm_x86.c|vm_ppc.c|vm_ppc_new.c|vm_sparc.c|vm_mips.c|vm_arm.c|sv_rankings.c) continue ;;
    esac
    sweep "$Q3_FILE"
done
for Q3_MP in -UMISSIONPACK -DMISSIONPACK; do
    for Q3_FILE in "$Q3_TEST_ROOT"/code/game/*.c; do
        case "$(basename "$Q3_FILE")" in g_rankings.c|bg_lib.c) continue ;; esac
        sweep "$Q3_FILE" "$Q3_MP" -DGAME_MODULE -DvmMain=Game_vmMain -DdllEntry=Game_dllEntry
    done
    for Q3_FILE in "$Q3_TEST_ROOT"/code/cgame/*.c; do
        if [ "$Q3_MP" = -UMISSIONPACK ] && [ "$(basename "$Q3_FILE")" = cg_newdraw.c ]; then
            continue
        fi
        sweep "$Q3_FILE" "$Q3_MP" -DCGAME_MODULE -DvmMain=CGame_vmMain -DdllEntry=CGame_dllEntry
    done
    for Q3_FILE in "$Q3_TEST_ROOT"/code/ui/*.c; do
        sweep "$Q3_FILE" "$Q3_MP" -DUI_MODULE -DvmMain=UI_vmMain -DdllEntry=UI_dllEntry
    done
done
sweep "$Q3_TEST_ROOT/code/ui/ui_shared.c" -DMISSIONPACK -DCGAME -DCGAME_MODULE \
    -DvmMain=CGame_vmMain -DdllEntry=CGame_dllEntry
for Q3_FILE in "$Q3_TEST_ROOT"/code/q3_ui/*.c; do
    case "$(basename "$Q3_FILE")" in
        ui_rankings.c|ui_rankstatus.c|ui_signup.c|ui_login.c|ui_specifyleague.c) continue ;;
    esac
    sweep "$Q3_FILE" -DUI_MODULE -DvmMain=UI_vmMain -DdllEntry=UI_dllEntry
done
echo "PASS: $Q3_SWEPT engine and module builds are format-clean"
