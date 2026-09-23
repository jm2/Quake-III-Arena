#!/usr/bin/env bash
# Fail when two objects of one monolithic link define the same global symbol.
#
# The Mac build links the engine (botlib included) and the game, cgame and UI
# modules into one XCOFF image. Retro68's BFD XCOFF linker silently ignores a
# redefinition that comes from an archive member, so two modules that define
# the same global share one object. In #233 botlib's `int g_gametype` wrote
# over the handle of the game's `vmCvar_t g_gametype`.
#
#   bash tests/run_duplicate_global_tests.sh
#       Host mode (CI): configures CMakeLists.txt with $CC,
#       -DBUILD_TEAM_ARENA=ON and stub Retro68 MakePEF and Rez tools, builds
#       the objects of every executable with CMake's own compile rules plus
#       -fno-common, and runs $NM (default nm). It never links, so the stubs
#       never run and no Retro68 toolchain is needed.
#   bash tests/run_duplicate_global_tests.sh --build-dir DIR
#       Retro68 mode: runs $NM (default powerpc-apple-macos-nm) over the
#       objects of a Unix Makefiles build tree made with the Retro68 toolchain.
#
# Both modes take each executable's objects, including the members of the
# module archives it links, from the link.txt files CMake generates, so the
# source lists, exclusions, definitions and targets always match
# CMakeLists.txt. Host mode undefines __MACOS__ and __POWERPC__, which select
# Mac Toolbox headers and PowerPC inline asm the host lacks. It supplies a
# generated <GL/gl.h> and <GL/glx.h> for code/renderer, and it cannot build
# code/mac (Toolbox headers exist only in Retro68). Retro68 mode covers those.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
export LC_ALL=C
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Duplicates that are known to be benign: symbol, then every defining object.
# The entry must match the defining objects exactly and must still occur.
Q3_ALLOWED_DUPLICATES='
# game and cgame each define this vmCvar_t and register it for the same
# engine cvar and default, so the shared handle and value suit both modules.
pmove_fixed cgame/cg_main.c game/g_main.c
pmove_msec cgame/cg_main.c game/g_main.c
# Identical copies: (style & UI_SMALLFONT) ? 0.75 (PROP_SMALL_SIZE_SCALE) : 1.0.
UI_ProportionalSizeScale cgame/cg_drawtools.c q3_ui/ui_atoms.c
'

Q3_BUILD_DIR=
Q3_HOST=1
while [ "$#" -gt 0 ]; do
    case "$1" in
        --build-dir) Q3_BUILD_DIR="$(cd "$2" && pwd)"; Q3_HOST=0; shift 2;;
        *) echo "Unknown argument: $1" >&2; exit 2;;
    esac
done

Q3_TEST_WORK="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-duplicate-globals.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_WORK"' EXIT

if [ "$Q3_HOST" -eq 0 ]; then
    Q3_MODE="Retro68 build tree $Q3_BUILD_DIR"
    NM="${NM:-powerpc-apple-macos-nm}"
else
    Q3_MODE="host ${CC:-cc}"
    command -v cmake > /dev/null || { echo "FAIL: host mode needs cmake" >&2; exit 1; }
    # qgl.h includes these on Linux. The renderer needs only the GL types and
    # constant names, and defined globals do not depend on constant values.
    mkdir -p "$Q3_TEST_WORK/gl/GL"
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
    } > "$Q3_TEST_WORK/gl/GL/gl.h"
    printf '%s\n' 'typedef struct _XDisplay Display; typedef struct XVisualInfo XVisualInfo;' \
        'typedef struct __GLXcontextRec *GLXContext; typedef unsigned long GLXDrawable; typedef int Bool;' \
        > "$Q3_TEST_WORK/gl/GL/glx.h"
    # CMakeLists.txt stops the configure unless it finds MakePEF, Rez and the
    # Rez includes under Rez's grandparent directory. Only the application
    # steps run the tools, and host mode never builds them, so stubs that fail
    # are enough.
    mkdir -p "$Q3_TEST_WORK/retro68/bin" "$Q3_TEST_WORK/retro68/universal/RIncludes"
    for tool in MakePEF Rez; do
        printf '#!/bin/sh\nexit 1\n' > "$Q3_TEST_WORK/retro68/bin/$tool"
        chmod +x "$Q3_TEST_WORK/retro68/bin/$tool"
    done
    : > "$Q3_TEST_WORK/retro68/universal/RIncludes/Types.r"
    : > "$Q3_TEST_WORK/retro68/universal/RIncludes/CodeFragments.r"
    Q3_BUILD_DIR="$Q3_TEST_WORK/build"
    if ! cmake -S "$Q3_TEST_ROOT" -B "$Q3_BUILD_DIR" -G "Unix Makefiles" \
            -DCMAKE_C_COMPILER="${CC:-cc}" -DBUILD_TEAM_ARENA=ON \
            -DRETRO68_MAKEPEF="$Q3_TEST_WORK/retro68/bin/MakePEF" \
            -DRETRO68_REZ="$Q3_TEST_WORK/retro68/bin/Rez" \
            "-DCMAKE_C_FLAGS=-U__MACOS__ -U__POWERPC__ -fno-common -w -I$Q3_TEST_WORK/gl" \
            > "$Q3_TEST_WORK/cmake.log" 2>&1; then
        cat "$Q3_TEST_WORK/cmake.log" >&2
        echo "FAIL: host CMake configure failed" >&2
        exit 1
    fi
fi
cd "$Q3_BUILD_DIR"

# Prints the objects named by a target's link.txt. An executable's list
# includes the objects of every archive of this build that it links.
q3_link_objects() {
    local target="$1" token library
    [ -f "CMakeFiles/$target.dir/link.txt" ] || { echo "missing CMakeFiles/$target.dir/link.txt" >&2; return 1; }
    for token in $(head -n 1 "CMakeFiles/$target.dir/link.txt" | tr -d '"'); do
        case "$token" in
            @*) tr -d '"' < "${token#@}" | tr ' ' '\n' | grep -E '\.(o|obj)$' || true;;
            *.o|*.obj) printf '%s\n' "$token";;
            lib*.a)
                library="${token#lib}"
                library="${library%.a}"
                if [ "$library" != "$target" ] && [ -f "CMakeFiles/$library.dir/link.txt" ]; then
                    q3_link_objects "$library"
                fi;;
        esac
    done
}

# An archive's link.txt names the archive itself; an executable's does not.
Q3_LINKS=()
for link_file in CMakeFiles/*.dir/link.txt; do
    [ -f "$link_file" ] || continue
    target="${link_file#CMakeFiles/}"
    target="${target%.dir/link.txt}"
    if ! head -n 1 "$link_file" | tr -d '"' | tr ' ' '\n' | grep -qxF "lib$target.a"; then
        Q3_LINKS+=("$target")
    fi
done
[ "${#Q3_LINKS[@]}" -gt 0 ] || { echo "FAIL: no executable link.txt under $Q3_BUILD_DIR/CMakeFiles" >&2; exit 1; }

declare -A Q3_LINK_FILES=()
for link in "${Q3_LINKS[@]}"; do
    q3_link_objects "$link" | sort -u > "$Q3_TEST_WORK/$link.objects"
    [ -s "$Q3_TEST_WORK/$link.objects" ] || { echo "FAIL: $link links no objects" >&2; exit 1; }
    if [ "$Q3_HOST" -eq 0 ]; then
        Q3_LINK_FILES[$link]="$(tr '\n' ' ' < "$Q3_TEST_WORK/$link.objects")"
    else
        Q3_LINK_FILES[$link]="$(grep -v '\.dir/code/mac/' "$Q3_TEST_WORK/$link.objects" | tr '\n' ' ')"
    fi
done

if [ "$Q3_HOST" -eq 1 ]; then
    # Build with CMake's own compile rules, one target directory at a time.
    # shellcheck disable=SC2086
    printf '%s\n' ${Q3_LINK_FILES[@]} | sort -u > "$Q3_TEST_WORK/host.objects"
    for target_dir in $(sed 's|^\(CMakeFiles/[^/]*\.dir\)/.*|\1|' "$Q3_TEST_WORK/host.objects" | sort -u); do
        # shellcheck disable=SC2046
        if ! make -j "$(nproc 2>/dev/null || echo 2)" -f "$target_dir/build.make" \
                $(grep -F "$target_dir/" "$Q3_TEST_WORK/host.objects") > "$Q3_TEST_WORK/make.log" 2>&1; then
            cat "$Q3_TEST_WORK/make.log" >&2
            echo "FAIL: host compilation of $target_dir failed" >&2
            exit 1
        fi
    done
fi

# Prints "symbol source-label" for every defined global of the given objects.
# Labels are the source path below code/, so both modes share the allowlist.
# XCOFF lists a function as its descriptor and its ".name" entry point.
q3_defined_globals() {
    "${NM:-nm}" -P -A -g --defined-only "$@" | awk '
        {
            label = $1; sub(/:$/, "", label); sub(/.*\.dir\//, "", label)
            sub(/^code\//, "", label); sub(/\.(obj|o)$/, "", label)
            name = $2; sub(/^\./, "", name)
            print name, label
        }' | sort -u
}

Q3_ALLOWED="$(printf '%s\n' "$Q3_ALLOWED_DUPLICATES" | sed -e '/^#/d' -e '/^$/d')"
Q3_ALLOWED_SEEN=""
Q3_FAILED=0
echo "Duplicate global check ($Q3_MODE)"
for link in "${Q3_LINKS[@]}"; do
    for object in ${Q3_LINK_FILES[$link]}; do
        [ -f "$object" ] || { echo "FAIL: $link object $object is missing" >&2; exit 1; }
    done
    # shellcheck disable=SC2086
    q3_defined_globals ${Q3_LINK_FILES[$link]} > "$Q3_TEST_WORK/$link.globals"
    # A parser or compile regression must not pass as "no duplicates".
    for canary in "GetBotLibAPI botlib/be_interface.c" "Com_Printf qcommon/common.c" \
            "RE_BeginFrame renderer/tr_cmds.c" "Game_vmMain game/g_main.c" \
            "CGame_vmMain cgame/cg_main.c"; do
        grep -qxF "$canary" "$Q3_TEST_WORK/$link.globals" || {
            echo "FAIL $link: expected global '$canary' not found; symbol scan is broken" >&2
            Q3_FAILED=1
        }
    done
    duplicates="$(awk '
        $1 != symbol { if (count > 1) print line; symbol = $1; line = $0; count = 1; next }
        { line = line " " $2; count++ }
        END { if (count > 1) print line }' "$Q3_TEST_WORK/$link.globals")"
    printf '%s: %s objects define %s globals' "$link" \
        "$(cut -d' ' -f2 "$Q3_TEST_WORK/$link.globals" | sort -u | wc -l)" \
        "$(wc -l < "$Q3_TEST_WORK/$link.globals")"
    if [ "$Q3_HOST" -eq 1 ]; then
        printf ' (%s code/mac objects not built on the host)' \
            "$(grep -c '\.dir/code/mac/' "$Q3_TEST_WORK/$link.objects" || true)"
    fi
    echo
    while read -r duplicate; do
        [ -n "$duplicate" ] || continue
        if grep -qxF "$duplicate" <<< "$Q3_ALLOWED"; then
            echo "  allowed: $duplicate"
            Q3_ALLOWED_SEEN+="$duplicate"$'\n'
        else
            echo "  DUPLICATE: $duplicate"
            Q3_FAILED=1
        fi
    done <<< "$duplicates"
done

while read -r allowed; do
    if ! grep -qxF "$allowed" <<< "$Q3_ALLOWED_SEEN"; then
        echo "FAIL allowlist entry no longer matches any link: $allowed" >&2
        Q3_FAILED=1
    fi
done <<< "$Q3_ALLOWED"

if [ "$Q3_FAILED" -ne 0 ]; then
    echo "FAIL: one global name must have exactly one definition per executable; rename or make static" >&2
    exit 1
fi
echo "PASS: no unexpected duplicate globals"
