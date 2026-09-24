#!/usr/bin/env bash
# Issue #398: list paths of 255, 256, 400 and 4096 characters through the real
# files.c (FS_GetFileList, FS_ListFiles, FS_ListFilteredFiles and the dir
# command) and through the real UI and game FS_GETFILELIST traps. 256 and
# longer once overflowed FS_ReturnPath's 256-byte stack buffer.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-fs-list-long-path.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
# pk3 directories plus names of 250 and 255 characters, built as the
# fixture's LongPath() builds its paths.
python3 - "$Q3_TEST_DIR/pak0.pk3" <<'PY_ZIP'
import sys, zipfile
def long_path(length, shift):
    path = "".join("/" if i % 100 == 99 else chr(ord("a") + (i + shift) % 26) for i in range(length))
    return path[:-1] + "z" if path.endswith("/") else path
entries = [
    ("scripts/base_wall.shader", b"shader\n"),
    ("scripts/sfx.shader", b"shader\n"),
    ("levelshots/q3dm1.jpg", b"shot\n"),
    ("maps/q3dm1.bsp", b"bsp\n"),
    (long_path(240, 0) + "/near.cfg", b"// near\n"),
    (long_path(246, 13) + "/edge.cfg", b"// edge\n"),
]
assert len(entries[-1][0]) == 255
with zipfile.ZipFile(sys.argv[1], "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for name, data in entries:
        archive.writestr(name, data)
PY_ZIP
for Q3_TEST_MODE in fs ui game; do
    Q3_TEST_EXTRA=()
    case "$Q3_TEST_MODE" in
        ui) Q3_TEST_EXTRA=(-DQ3_TEST_UI "$Q3_TEST_ROOT/code/qcommon/vm.c") ;;
        game) Q3_TEST_EXTRA=(-DQ3_TEST_GAME "$Q3_TEST_ROOT/code/qcommon/vm.c" "$Q3_TEST_ROOT/code/game/q_math.c") ;;
    esac
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "$Q3_TEST_ROOT/tests/fs_list_long_path_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/unzip.c" "$Q3_TEST_ROOT/code/qcommon/md4.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "${Q3_TEST_EXTRA[@]}" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/fs-list-long-path-$Q3_TEST_MODE"
    ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/fs-list-long-path-$Q3_TEST_MODE" "$Q3_TEST_DIR/pak0.pk3" "$Q3_TEST_DIR"
done
