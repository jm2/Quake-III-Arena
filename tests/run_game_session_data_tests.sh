#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-game-session-data.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Link the real native g_session.c (G_ReadSessionData, G_WriteClientSessionData,
# G_InitSessionData) and g_active.c (SpectatorClientEndFrame) as the monolithic
# static build does. Out-of-range, garbage and short "sessionN" cvar strings
# must read back in range, and a spectator following what was read must stay
# inside level.clients; valid session data must round-trip unchanged. Built
# for base Quake III and Team Arena (MISSIONPACK), which compile the same
# files.
for Q3_TEST_VARIANT in base missionpack; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_VARIANT" == missionpack ]]; then
        Q3_TEST_FLAGS=(-DMISSIONPACK)
    fi
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/game_session_data_regression.c" \
        "$Q3_TEST_ROOT/code/game/g_session.c" \
        "$Q3_TEST_ROOT/code/game/g_active.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_VARIANT"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_VARIANT"
done
