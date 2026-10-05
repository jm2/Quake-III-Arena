#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-mac-syscalls.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #258: the File Manager permission code/mac/mac_syscalls.c's _open_r
# asks for.  The regression includes that file whole; its <reent.h>,
# <Files.h> and <Errors.h> resolve to tests/mac_files_fake.h.
for Q3_TEST_HEADER in reent.h Files.h Errors.h; do
    printf '#include "%s/tests/mac_files_fake.h"\n' "$Q3_TEST_ROOT" > "$Q3_TEST_DIR/$Q3_TEST_HEADER"
done

"${CC:-cc}" \
    -std=gnu99 -Wall -Wno-multichar -Wno-unused-function -fno-omit-frame-pointer \
    -fsanitize=address,undefined -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/mac_syscalls_regression.c" \
    -o "$Q3_TEST_DIR/mac_syscalls"

ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/mac_syscalls" read read-locked-file read-locked-volume read-cd \
        read-twice read-missing read-truncate write write-new write-locked \
        append update exclusive hopen-fallback long-name
