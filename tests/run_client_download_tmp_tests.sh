#!/usr/bin/env bash
# Review of #494 (issue #327): the Mac refuses path components longer than
# HFS's 31 bytes, so CL_BeginDownload's "<pk3 name>.tmp" made pk3s named in
# 28 to 31 bytes undownloadable there.  The real CL_BeginDownload names the
# files and the real code/mac/mac_syscalls.c (on tests/mac_files_fake.h)
# creates and renames them; built without the Mac PATH_SEP, the temporary
# name must stay "<name>.tmp" as in 1.32c.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-client-download-tmp.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# mac_syscalls.c's <reent.h>, <Files.h>, <Errors.h> and <StringCompare.h>
# resolve to the fake; its types clash with q_shared.h's, so it is a
# separate object.
for Q3_TEST_HEADER in reent.h Files.h Errors.h StringCompare.h; do
    printf '#include "%s/tests/mac_files_fake.h"\n' "$Q3_TEST_ROOT" > "$Q3_TEST_DIR/$Q3_TEST_HEADER"
done
"${CC:-cc}" -std=gnu99 -Wall -Wno-multichar -Wno-unused-function -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined -I"$Q3_TEST_DIR" \
    -DQ3_TEST_MAC_SIDE -c "$Q3_TEST_ROOT/tests/client_download_tmp_regression.c" \
    -o "$Q3_TEST_DIR/mac_side.o"

for Q3_TEST_MODE in hfs host; do
    Q3_TEST_FLAGS=()
    if [ "$Q3_TEST_MODE" = hfs ]; then
        Q3_TEST_FLAGS=(-DQ3_TEST_HFS)
    fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/client_download_tmp_regression.c" "$Q3_TEST_DIR/mac_side.o" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/client_download_tmp_$Q3_TEST_MODE"
    ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/client_download_tmp_$Q3_TEST_MODE"
done
