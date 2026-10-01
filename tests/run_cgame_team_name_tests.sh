#!/usr/bin/env bash
# Issue #444: the Team Arena cgame put the server's team name and a '/' in a
# MAX_QPATH buffer. Load players with no team name, a normal one, and names of
# 62 (the longest that fits with its slash) to 255 characters (the cvar limit)
# through the real cgame, with AddressSanitizer on its stack buffers.
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-cgame-team-name.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Link the whole native cgame, as the monolithic build does, behind a fake
# syscall table: base Quake III and Team Arena (MISSIONPACK) variants.
for Q3_TEST_VARIANT in base missionpack; do
    Q3_TEST_FLAGS=()
    Q3_TEST_SOURCES=()
    for Q3_TEST_SOURCE in "$Q3_TEST_ROOT"/code/cgame/cg_*.c; do
        # CMake builds cg_newdraw.c only into the Team Arena cgame.
        if [[ "$Q3_TEST_VARIANT" == base && "$Q3_TEST_SOURCE" == */cg_newdraw.c ]]; then
            continue
        fi
        Q3_TEST_SOURCES+=("$Q3_TEST_SOURCE")
    done
    if [[ "$Q3_TEST_VARIANT" == missionpack ]]; then
        Q3_TEST_FLAGS=(-DMISSIONPACK)
        Q3_TEST_SOURCES+=("$Q3_TEST_ROOT/code/ui/ui_shared.c")
    fi
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/cgame_team_name_regression.c" "${Q3_TEST_SOURCES[@]}" \
        "$Q3_TEST_ROOT/code/game/bg_misc.c" "$Q3_TEST_ROOT/code/game/bg_pmove.c" \
        "$Q3_TEST_ROOT/code/game/bg_slidemove.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_VARIANT"

    # One process per case. LeakSanitizer cannot initialize in the local ptrace sandbox.
    for Q3_TEST_CASE in normal 62 63 64 100 255; do
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_VARIANT" "$Q3_TEST_CASE"
    done
done
