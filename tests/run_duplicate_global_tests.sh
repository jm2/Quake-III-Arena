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
#       Host mode (CI): compiles each CMakeLists.txt source group with $CC and
#       -fno-common and runs $NM (default nm) over the objects.
#   bash tests/run_duplicate_global_tests.sh --build-dir DIR
#       Retro68 mode: runs $NM (default powerpc-apple-macos-nm) over the
#       objects and module archives of a CMake build tree. Use a clean tree;
#       stale objects are included.
#
# Host mode mirrors the CMake GLOBs and REMOVE_ITEM lists, and the global and
# per-module definitions except -D__MACOS__ -D__POWERPC__, which select Mac
# Toolbox headers and PowerPC inline asm the host lacks. It leaves out
# code/mac/*.c (Toolbox headers exist only in Retro68) and, when the host has
# no <GL/gl.h> and <GL/glx.h>, code/renderer/*.c. Retro68 mode covers both.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
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
while [ "$#" -gt 0 ]; do
    case "$1" in
        --build-dir) Q3_BUILD_DIR="$(cd "$2" && pwd)"; shift 2;;
        *) echo "Unknown argument: $1" >&2; exit 2;;
    esac
done

Q3_TEST_WORK="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-duplicate-globals.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_WORK"' EXIT

# Compiles one source (relative to the tree root) for one CMake target.
q3_compile_object() {
    local target="$1" source="$2" work="$3" object log
    local defines=(-DBOTLIB -DQ3_STATIC '-DQUAKEDATA="/Quake3/"')
    case "$target" in
        game_obj|game_mp_obj) defines+=(-DvmMain=Game_vmMain -DdllEntry=Game_dllEntry -DGAME_MODULE);;
        cgame_obj|cgame_mp_obj) defines+=(-DvmMain=CGame_vmMain -DdllEntry=CGame_dllEntry -DCGAME_MODULE);;
        q3ui_lib|ui_mp_obj) defines+=(-DvmMain=UI_vmMain -DdllEntry=UI_dllEntry -DUI_MODULE);;
    esac
    case "$target" in *_mp_obj) defines+=(-DMISSIONPACK);; esac
    object="$work/$target/$source.o"
    log="$object.log"
    mkdir -p "$(dirname "$object")"
    if ! "${CC:-cc}" -std=gnu99 -fgnu89-inline -O0 -fno-strict-aliasing -fno-common -w "${defines[@]}" \
            -Icode/game -Icode/cgame -Icode/ui -Icode/client -Icode/server -Icode/renderer \
            -Icode/qcommon -Icode/botlib -Icode/mac -Icode \
            -c "$source" -o "$object" > "$log" 2>&1; then
        echo "compile failed: $target $source" >&2
        cat "$log" >&2
        return 1
    fi
}
export -f q3_compile_object

# Prints "symbol source-label" for every defined global of the given files.
# Labels are the source path below code/, so both modes share the allowlist.
# XCOFF lists a function as its descriptor and its ".name" entry point.
q3_defined_globals() {
    "${NM:-nm}" -P -A -g --defined-only "$@" | awk '
        {
            file = $1; sub(/:$/, "", file); name = $2; sub(/^\./, "", name)
            if (match(file, /\[[^]]*\]$/)) {
                member = substr(file, RSTART + 1, RLENGTH - 2)
                library = substr(file, 1, RSTART - 1); sub(/.*\//, "", library)
                if (library ~ /^libcgame(_mp)?_obj\.a$/) dir = "cgame"
                else if (library ~ /^libgame(_mp)?_obj\.a$/) dir = "game"
                else if (library == "libq3ui_lib.a") dir = "q3_ui"
                else if (library ~ /^libui(_mp)?_obj\.a$/) dir = "ui"
                else dir = library
                label = dir "/" member
            } else {
                label = file; sub(/.*\/code\//, "", label)
            }
            sub(/\.(obj|o)$/, "", label)
            print name, label
        }' | LC_ALL=C sort -u
}

declare -A Q3_LINK_FILES=()
Q3_LINKS=(Quake3 Quake3_TeamArena)
cd "$Q3_TEST_ROOT"

if [ -n "$Q3_BUILD_DIR" ]; then
    Q3_MODE="Retro68 build tree $Q3_BUILD_DIR"
    NM="${NM:-powerpc-apple-macos-nm}"
    Q3_LINK_LIBRARIES_Quake3="libgame_obj.a libcgame_obj.a libq3ui_lib.a"
    Q3_LINK_LIBRARIES_Quake3_TeamArena="libgame_mp_obj.a libcgame_mp_obj.a libui_mp_obj.a"
    Q3_PRESENT_LINKS=()
    for link in "${Q3_LINKS[@]}"; do
        [ -d "$Q3_BUILD_DIR/CMakeFiles/$link.dir" ] || continue
        libraries_var="Q3_LINK_LIBRARIES_$link"
        files="$(find "$Q3_BUILD_DIR/CMakeFiles/$link.dir" -name '*.obj' | LC_ALL=C sort | tr '\n' ' ')"
        for library in ${!libraries_var}; do
            [ -f "$Q3_BUILD_DIR/$library" ] || { echo "missing $Q3_BUILD_DIR/$library" >&2; exit 1; }
            files+="$Q3_BUILD_DIR/$library "
        done
        Q3_LINK_FILES[$link]="$files"
        Q3_PRESENT_LINKS+=("$link")
    done
    [ "${#Q3_PRESENT_LINKS[@]}" -gt 0 ] || { echo "no CMakeFiles/Quake3*.dir in $Q3_BUILD_DIR" >&2; exit 1; }
    Q3_LINKS=("${Q3_PRESENT_LINKS[@]}")
else
    Q3_MODE="host ${CC:-cc}"
    shopt -s nullglob
    # Excludes a CMake REMOVE_ITEM list (basenames) from the named glob.
    q3_sources() {
        local pattern="$1" source skip; shift
        for source in $pattern; do
            for skip in "$@"; do [ "${source##*/}" = "$skip" ] && continue 2; done
            printf '%s\n' "$source"
        done
    }
    Q3_ENGINE_SOURCES="$(
        q3_sources 'code/qcommon/*.c' vm_x86.c vm_ppc.c vm_ppc_new.c vm_sparc.c vm_mips.c vm_arm.c
        q3_sources 'code/server/*.c' sv_rankings.c
        q3_sources 'code/client/*.c'
        if printf '#include <GL/gl.h>\n#include <GL/glx.h>\n' | "${CC:-cc}" -E -x c - > /dev/null 2>&1; then
            q3_sources 'code/renderer/*.c'
        fi
        q3_sources 'code/botlib/*.c'
        q3_sources 'code/jpeg-6/*.c' jmemdos.c jmemansi.c jmemname.c jpegtran.c
    )"
    case "$Q3_ENGINE_SOURCES" in
        *code/renderer/*) ;;
        *) echo "note: host has no <GL/gl.h> and <GL/glx.h>; code/renderer is not checked";;
    esac
    Q3_GAME_SOURCES="$(q3_sources 'code/game/*.c' g_rankings.c)"
    Q3_TARGET_SOURCES_game_obj="$Q3_GAME_SOURCES"
    Q3_TARGET_SOURCES_game_mp_obj="$Q3_GAME_SOURCES"
    Q3_TARGET_SOURCES_cgame_obj="$(q3_sources 'code/cgame/*.c' cg_newdraw.c)"
    Q3_TARGET_SOURCES_cgame_mp_obj="$(q3_sources 'code/cgame/*.c')"
    Q3_TARGET_SOURCES_q3ui_lib="$(q3_sources 'code/q3_ui/*.c' ui_rankings.c ui_rankstatus.c ui_signup.c ui_login.c ui_specifyleague.c)"
    Q3_TARGET_SOURCES_ui_mp_obj="$(q3_sources 'code/ui/*.c')"
    Q3_TARGET_SOURCES_Quake3="$Q3_ENGINE_SOURCES"
    Q3_LINK_TARGETS_Quake3="Quake3 game_obj cgame_obj q3ui_lib"
    # The Team Arena executable shares the engine objects of Quake3.
    Q3_LINK_TARGETS_Quake3_TeamArena="Quake3 game_mp_obj cgame_mp_obj ui_mp_obj"

    if ! for target in Quake3 game_obj cgame_obj q3ui_lib game_mp_obj cgame_mp_obj ui_mp_obj; do
            sources_var="Q3_TARGET_SOURCES_$target"
            for source in ${!sources_var}; do printf '%s\0%s\0' "$target" "$source"; done
        done | xargs -0 -n 2 -P "$(nproc 2>/dev/null || echo 2)" \
            bash -c 'q3_compile_object "$2" "$3" "$1"' _ "$Q3_TEST_WORK"; then
        echo "FAIL: host compilation of the CMake source groups failed" >&2
        exit 1
    fi

    for link in "${Q3_LINKS[@]}"; do
        targets_var="Q3_LINK_TARGETS_$link"
        files=""
        for target in ${!targets_var}; do
            sources_var="Q3_TARGET_SOURCES_$target"
            for source in ${!sources_var}; do
                [ -f "$Q3_TEST_WORK/$target/$source.o" ] || { echo "missing object for $target $source" >&2; exit 1; }
                files+="$Q3_TEST_WORK/$target/$source.o "
            done
        done
        Q3_LINK_FILES[$link]="$files"
    done
fi

Q3_ALLOWED="$(printf '%s\n' "$Q3_ALLOWED_DUPLICATES" | sed -e '/^#/d' -e '/^$/d')"
Q3_ALLOWED_SEEN=""
Q3_FAILED=0
echo "Duplicate global check ($Q3_MODE)"
for link in "${Q3_LINKS[@]}"; do
    # shellcheck disable=SC2086
    q3_defined_globals ${Q3_LINK_FILES[$link]} > "$Q3_TEST_WORK/$link.globals"
    # A parser or compile regression must not pass as "no duplicates".
    for canary in "GetBotLibAPI botlib/be_interface.c" "Com_Printf qcommon/common.c" \
            "Game_vmMain game/g_main.c" "CGame_vmMain cgame/cg_main.c"; do
        grep -qxF "$canary" "$Q3_TEST_WORK/$link.globals" || {
            echo "FAIL $link: expected global '$canary' not found; symbol scan is broken" >&2
            Q3_FAILED=1
        }
    done
    duplicates="$(awk '
        $1 != symbol { if (count > 1) print line; symbol = $1; line = $0; count = 1; next }
        { line = line " " $2; count++ }
        END { if (count > 1) print line }' "$Q3_TEST_WORK/$link.globals")"
    printf '%s: %s objects define %s globals\n' "$link" \
        "$(cut -d' ' -f2 "$Q3_TEST_WORK/$link.globals" | sort -u | wc -l)" \
        "$(wc -l < "$Q3_TEST_WORK/$link.globals")"
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
