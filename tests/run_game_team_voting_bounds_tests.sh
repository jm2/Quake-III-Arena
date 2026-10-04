#!/usr/bin/env bash
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-game-team-voting-bounds.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #463: link the real native g_main.c, as the monolithic static build
# does, and run CalculateRanks and CheckTeamVote. UBSan's array bounds check
# runs with no recovery, so a write past level.numteamVotingClients[2] stops
# the test. --gc-sections keeps only the paths the test calls; the test stubs
# the engine traps and game symbols they reference. Built for base Quake III
# and Team Arena (MISSIONPACK).
for Q3_TEST_VARIANT in base missionpack; do
    Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
        -Wno-pointer-to-int-cast -fsanitize=address,undefined -fsanitize=bounds
        -fno-sanitize-recover=all)
    if [[ "$Q3_TEST_VARIANT" == missionpack ]]; then
        Q3_TEST_FLAGS+=(-DMISSIONPACK)
    fi
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/game_team_voting_bounds_regression.c" \
        "$Q3_TEST_ROOT/code/game/g_main.c" \
        "$Q3_TEST_ROOT/code/game/q_math.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_VARIANT"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_VARIANT"
done
