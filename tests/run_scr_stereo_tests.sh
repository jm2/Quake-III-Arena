#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-scr-stereo.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #30: in stereo, each eye gets a whole screen field (begin, cgame or
# cinematic or UI, console) before the one EndFrame. cl_scrn.c needs the whole
# client, so the fixture takes SCR_DrawScreenField and SCR_UpdateScreen from it
# verbatim (with a #line so reports name that file) and records the renderer,
# UI VM and client calls they make.
Q3_TEST_EXTRACT() {
    awk -v start="$1" -v file="$2" '
        index($0, start) == 1 { printf "#line %d \"%s\"\n", NR, file; found = 1 }
        found { print }
        found && /^}/ { done = 1; exit }
        END { exit !done }
    ' "$2" >> "$3" || {
        echo "run_scr_stereo_tests: no $1 in $2" >&2
        exit 1
    }
}
Q3_TEST_SCRN="$Q3_TEST_ROOT/code/client/cl_scrn.c"
Q3_TEST_EXTRACT 'void SCR_DrawScreenField(' "$Q3_TEST_SCRN" "$Q3_TEST_DIR/scr_stereo_bodies.c"
Q3_TEST_EXTRACT 'void SCR_UpdateScreen(' "$Q3_TEST_SCRN" "$Q3_TEST_DIR/scr_stereo_bodies.c"

"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/scr_stereo_regression.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/scr_stereo"

# LeakSanitizer cannot initialize in the local ptrace sandbox.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/scr_stereo"
