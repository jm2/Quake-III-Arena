#!/usr/bin/env bash
# Issue #259: FS_SV_Rename's and FS_Rename's copy fallback (FS_CopyFile, then
# FS_Remove) when rename() fails, as it always did on the Mac before
# code/mac/mac_syscalls.c.  A copy whose whole-file malloc fails must not
# crash or remove the source; a download must still produce its pk3 on the
# fallback path; refused renames stay refused.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-fs-copy-rename.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
mkdir "$Q3_TEST_DIR/work"
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/fs_copy_rename_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/unzip.c" "$Q3_TEST_ROOT/code/qcommon/md4.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/fs_copy_rename"
ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/fs_copy_rename" "$Q3_TEST_DIR/work"
