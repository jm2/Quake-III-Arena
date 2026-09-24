#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-mac-net.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issues #277 and #265: the Mac Open Transport receive and resolve paths.
# mac_net.c needs the whole Mac Toolbox, so the fixture takes Sys_StringToAdr
# and Sys_GetPacket from it verbatim (with a #line so reports name that file),
# and Com_EventLoop from common.c; tests/mac_ot_fake.h stands in for OT.
Q3_TEST_EXTRACT() {
    awk -v start="$1" -v file="$2" '
        $0 ~ start { printf "#line %d \"%s\"\n", NR, file; found = 1 }
        found { print }
        found && /^}/ { found = 0; count++ }
        END { exit count == 1 ? 0 : 1 }
    ' "$2" >> "$3" || { echo "run_mac_net_tests: no single $1 in $2" >&2; exit 1; }
}
Q3_TEST_EXTRACT '^qboolean[[:space:]]+Sys_StringToAdr[[:space:]]*[(]' \
    "$Q3_TEST_ROOT/code/mac/mac_net.c" "$Q3_TEST_DIR/mac_net_extracted.c"
Q3_TEST_EXTRACT '^qboolean[[:space:]]+Sys_GetPacket[[:space:]]*[(]' \
    "$Q3_TEST_ROOT/code/mac/mac_net.c" "$Q3_TEST_DIR/mac_net_extracted.c"
Q3_TEST_EXTRACT '^int[[:space:]]+Com_EventLoop[[:space:]]*[(]' \
    "$Q3_TEST_ROOT/code/qcommon/common.c" "$Q3_TEST_DIR/com_event_loop_extracted.c"

"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/mac_net_regression.c" \
    -Wl,--gc-sections -o "$Q3_TEST_DIR/mac_net"

# One process per case: AddressSanitizer stops at the first overflow.
for Q3_TEST_CASE in host-normal host-255 host-256 host-1023 \
        packet-normal packet-split packet-full packet-drain-error event-oversize; do
    ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/mac_net" "$Q3_TEST_CASE"
done
