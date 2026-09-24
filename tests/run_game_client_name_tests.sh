#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-game-client-name.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Link the real native g_cmds.c (SanitizeString, ClientNumberFromString,
# ClientCommand, Cmd_Follow_f) as the monolithic static build does. The test
# calls the name matching on exact-size heap strings ending in ESC and '^' and
# sends "follow <name>" through ClientCommand. It stubs the engine traps and
# the game symbols the file's other commands reference. Built for base
# Quake III and Team Arena (MISSIONPACK), which compile the same g_cmds.c.
for Q3_TEST_VARIANT in base missionpack; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_VARIANT" == missionpack ]]; then
        Q3_TEST_FLAGS=(-DMISSIONPACK)
    fi
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/game_client_name_regression.c" \
        "$Q3_TEST_ROOT/code/game/g_cmds.c" \
        "$Q3_TEST_ROOT/code/game/q_math.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_VARIANT"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_VARIANT"
done
