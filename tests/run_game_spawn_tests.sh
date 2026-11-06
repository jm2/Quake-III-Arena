#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-game-spawn.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Include the real native g_client.c, for its static spawn sizes: base
# Quake III and Team Arena, which compile the same file. --gc-sections drops the
# code that does not take part in choosing a spawn point.
# -fno-sanitize=null because FOFS takes offsets from a null pointer, which clang traps.
for Q3_TEST_VARIANT in base missionpack; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_VARIANT" == missionpack ]]; then
        Q3_TEST_FLAGS=(-DMISSIONPACK)
    fi
    "${CC:-cc}" \
        -std=gnu99 -fsigned-char -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined -fno-sanitize=null "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/game_spawn_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_math.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_VARIANT"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_VARIANT"
done
