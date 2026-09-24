#!/usr/bin/env bash
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-snd-evict.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

# The test includes the real snd_dma.c and links the real snd_mem.c, snd_mix.c,
# snd_adpcm.c and snd_wavelet.c: S_PaintChannels mixes what eviction leaves.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    "$Q3_TEST_ROOT/tests/snd_evict_regression.c" "$Q3_TEST_ROOT/code/client/snd_mem.c" \
    "$Q3_TEST_ROOT/code/client/snd_mix.c" "$Q3_TEST_ROOT/code/client/snd_adpcm.c" \
    "$Q3_TEST_ROOT/code/client/snd_wavelet.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

# Each mode runs in its own process so a report in one does not hide the
# others, under a timeout in case a load waits forever for a sound to free.
for Q3_TEST_MODE in channel loop full reload mixer unused; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    if ! ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        timeout -k 10 120 "$Q3_TEST_BINARY" "$Q3_TEST_MODE"; then
        echo "sound eviction regression failed or timed out: $Q3_TEST_MODE" >&2
        exit 1
    fi
done
