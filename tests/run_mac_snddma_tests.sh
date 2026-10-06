#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-mac-snddma.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #264: the classic Mac Sound Manager backend must queue its buffers at
# the rate it reports to the mixer in dma.speed.  The fixture #includes the
# real code/mac/mac_snddma.c and supplies the Sound Manager;
# tests/mac_sound_fake.h stands in for Universal Interfaces' Sound.h.
# -Wno-pointer-to-int-cast: mac_snddma.c passes &s_sndHeader in SndCommand's
# 32-bit param2 as (int), which only fits on the 32-bit target.
cp "$Q3_TEST_ROOT/tests/mac_sound_fake.h" "$Q3_TEST_DIR/Sound.h"
"${CC:-cc}" \
    -std=gnu99 -fsigned-char -fno-omit-frame-pointer -fsanitize=address,undefined \
    -Wno-pointer-to-int-cast -I"$Q3_TEST_DIR" "$Q3_TEST_ROOT/tests/mac_snddma_regression.c" \
    -lm -o "$Q3_TEST_DIR/mac_snddma"

ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/mac_snddma"
