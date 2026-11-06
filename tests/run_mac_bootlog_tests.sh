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

# What feeds the log. The fixtures that run main and Sys_Error have their own
# Sys_LogRecord, so check the real ones' text.
Q3_TEST_BODY() {
    awk -v start="$1" '$0 ~ start { found = 1 } found { print } found && /^}/ { exit }' "$2"
}
Q3_TEST_BODY '^void Sys_LogRecord' "$Q3_TEST_ROOT/code/mac/mac_main.c" | grep -q 'Sys_BootLogWrite( text )' ||
    { echo "Sys_LogRecord does not feed the boot log" >&2; exit 1; }
Q3_TEST_BODY '^void QDECL Com_FlightRecord' "$Q3_TEST_ROOT/code/qcommon/common.c" | grep -q 'Sys_BootLogWrite( text )' ||
    { echo "Com_FlightRecord does not feed the boot log" >&2; exit 1; }
