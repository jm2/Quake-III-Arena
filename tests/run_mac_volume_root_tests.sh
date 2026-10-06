#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-mac-volume-root.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #267: a game folder at the root of a volume.  mac_main.c needs the
# whole Mac Toolbox, so the fixture takes Sys_GetCwd, Sys_JoinHFSPath and the
# catalog listing from it verbatim (with a #line so reports name that file),
# with FS_BuildOSPath from files.c and the filters Sys_ListFiles uses from
# common.c, and fakes the File Manager and Process Manager around them.
Q3_TEST_EXTRACT() {
    awk -v start="$1" -v file="$2" '
        $0 ~ start { printf "#line %d \"%s\"\n", NR, file; found = 1 }
        found { sub(/\r$/, ""); print }
        found && /^}/ { found = 0; count++ }
        END { exit count == 1 ? 0 : 1 }
    ' "$2" >> "$3" || { echo "run_mac_volume_root_tests: no single $1 in $2" >&2; exit 1; }
}
Q3_TEST_MAIN="$Q3_TEST_ROOT/code/mac/mac_main.c"
Q3_TEST_OUT="$Q3_TEST_DIR/mac_volume_root_extracted.c"
awk '/^#define[[:space:]]+MAX_FOUND_FILES[[:space:]]/ { sub(/\r$/, ""); print }' "$Q3_TEST_MAIN" > "$Q3_TEST_OUT"
[ "$(wc -l < "$Q3_TEST_OUT")" -eq 1 ] ||
    { echo "run_mac_volume_root_tests: no single MAX_FOUND_FILES in $Q3_TEST_MAIN" >&2; exit 1; }
Q3_TEST_EXTRACT '^char[[:space:]]*[*]Com_StringContains[[:space:]]*[(]' "$Q3_TEST_ROOT/code/qcommon/common.c" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^int[[:space:]]+Com_Filter[[:space:]]*[(]' "$Q3_TEST_ROOT/code/qcommon/common.c" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^int[[:space:]]+Com_FilterPath[[:space:]]*[(]' "$Q3_TEST_ROOT/code/qcommon/common.c" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^static void[[:space:]]+FS_ReplaceSeparators[[:space:]]*[(]' "$Q3_TEST_ROOT/code/qcommon/files.c" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^char[[:space:]]*[*]FS_BuildOSPath[[:space:]]*[(]' "$Q3_TEST_ROOT/code/qcommon/files.c" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^int[[:space:]]+PStringToCString[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^char[[:space:]]*[*]Sys_GetCwd[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^static void[[:space:]]+Sys_JoinHFSPath[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^static OSErr[[:space:]]+PathToFSSpec[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^static qboolean[[:space:]]+Sys_GetDirectoryID[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^static void[[:space:]]+Sys_ListFilteredDirectory[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_OUT"
Q3_TEST_EXTRACT '^char[[:space:]]*[*][*]Sys_ListFiles[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_OUT"

"${CC:-cc}" \
    -std=gnu99 -fsigned-char -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/mac_volume_root_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/mac_volume_root"

for Q3_TEST_CASE in root folder getvol; do
    ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/mac_volume_root" "$Q3_TEST_CASE"
done
