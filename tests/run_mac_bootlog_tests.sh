#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-mac-bootlog.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# code/mac/mac_bootlog.c, included whole by the regression; its <Files.h>,
# <Devices.h> and <Errors.h> resolve to tests/mac_bootlog_fake.h.
for Q3_TEST_HEADER in Files.h Devices.h Errors.h; do
    printf '#include "%s/tests/mac_bootlog_fake.h"\n' "$Q3_TEST_ROOT" > "$Q3_TEST_DIR/$Q3_TEST_HEADER"
done

"${CC:-cc}" \
    -std=gnu99 -Wall -Wno-multichar -Wno-unused-function -fno-omit-frame-pointer \
    -fsanitize=address,undefined -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/mac_bootlog_regression.c" \
    -o "$Q3_TEST_DIR/mac_bootlog"

ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/mac_bootlog"
