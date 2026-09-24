#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-connect-screen.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #419: the Team Arena UI (code/ui, MISSIONPACK) is linked natively, so
# the fixture includes the real ui_main.c and draws its connect screen with
# the server names connect can give it.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/ui_connect_screen_regression.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_connect_screen"

# One process per case: ASan stops at the first write past the screen's text.
# "Connecting to " and a name of up to 241 bytes fill the 256-byte text whole;
# 242 and up (255 is the most cls.servername holds, 1023 the most the UI's
# client state does) must be cut to fit. "normal" draws real names, which must
# be drawn byte for byte as before, and "download" a download name as long as
# the screen's copy of cl_downloadName holds.
for Q3_TEST_CASE in "servername 0" "servername 14" "servername 241" "servername 242" \
    "servername 255" "servername 1023" normal download; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    # shellcheck disable=SC2086 # the case is a mode and its argument
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/ui_connect_screen" $Q3_TEST_CASE
done
