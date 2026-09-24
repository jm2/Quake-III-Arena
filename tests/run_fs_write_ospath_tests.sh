#!/usr/bin/env bash
# Issue #414: writes, appends and renames (VM, FS_WriteFile, FS_SV_* and the
# download finalisation) whose OS path is MAX_OSPATH - 1, MAX_OSPATH or
# MAX_OSPATH + 1 characters must never be cut into a .pk3 or .qvm name.
# target: MAX_OSPATH 256 as on Retro68; hfs: that plus PATH_SEP ':';
# host: the host PATH_MAX.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-fs-write-ospath.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
for Q3_TEST_VARIANT in target hfs host; do
    for Q3_TEST_MODE in normal fast; do
        Q3_TEST_FLAGS=()
        case "$Q3_TEST_VARIANT" in
            hfs) Q3_TEST_FLAGS+=(-DQ3_TEST_HFS) ;;
            host) Q3_TEST_FLAGS+=(-DQ3_TEST_HOST_OSPATH) ;;
        esac
        if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS+=(-O2 -DNDEBUG -ffast-math); fi
        Q3_TEST_WORK="$Q3_TEST_DIR/$Q3_TEST_VARIANT-$Q3_TEST_MODE"
        mkdir "$Q3_TEST_WORK"
        "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
            -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
            "$Q3_TEST_ROOT/tests/fs_write_ospath_regression.c" \
            "$Q3_TEST_ROOT/code/qcommon/unzip.c" "$Q3_TEST_ROOT/code/qcommon/md4.c" \
            "$Q3_TEST_ROOT/code/game/q_shared.c" \
            -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/write-ospath-$Q3_TEST_VARIANT-$Q3_TEST_MODE"
        ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
            UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_DIR/write-ospath-$Q3_TEST_VARIANT-$Q3_TEST_MODE" "$Q3_TEST_WORK"
    done
done
