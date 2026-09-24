#!/usr/bin/env bash
# Issue #301: the scoreboard READY markers of the Quake3 and Team Arena cgames
# for clients 0-63, with -fsanitize=undefined (which includes shift).
set -euo pipefail
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-cgame-ready-mask.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# q3_ready_mask_test NAME CFLAGS... -- EXTRA_SOURCES...
q3_ready_mask_test() {
    local name="$1" flags=() sources=()
    shift
    while [ "$#" -gt 0 ] && [ "$1" != "--" ]; do
        flags+=("$1")
        shift
    done
    [ "$#" -gt 0 ] && shift
    sources=("$@")

    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${flags[@]}" \
        "$Q3_TEST_ROOT/tests/cgame_ready_mask_regression.c" \
        "${sources[@]}" \
        "$Q3_TEST_ROOT/code/game/q_math.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$name"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$name"
}

q3_ready_mask_test scoreboard --
q3_ready_mask_test scoreboard_ta -DMISSIONPACK --
q3_ready_mask_test feeder_ta -DMISSIONPACK -DREADY_FEEDER -- \
    "$Q3_TEST_ROOT/code/cgame/cg_scoreboard.c"
