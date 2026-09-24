#!/usr/bin/env bash
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-snd-rate.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

# The test includes the real snd_dma.c and links the real snd_mem.c and
# snd_adpcm.c. float-cast-overflow is named explicitly: GCC's undefined group
# leaves it out, and it is how a 0 Hz or 1 Hz sound's sample count fails.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all \
    "$Q3_TEST_ROOT/tests/snd_rate_regression.c" "$Q3_TEST_ROOT/code/client/snd_mem.c" \
    "$Q3_TEST_ROOT/code/client/snd_adpcm.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

# Each mode runs in its own process so a report in one does not hide the
# others, under a timeout: the unfixed sound code loops forever in several.
for Q3_TEST_MODE in music-format music-stream music-slow music-tiny music-empty \
    sfx-rate sfx-pool sfx-malloc sfx-reload sfx-length adpcm-empty valid; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    if ! ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        timeout -k 10 120 "$Q3_TEST_BINARY" "$Q3_TEST_MODE"; then
        echo "sound rate regression failed or timed out: $Q3_TEST_MODE" >&2
        exit 1
    fi
done
