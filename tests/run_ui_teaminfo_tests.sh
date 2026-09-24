#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-teaminfo.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# The Team Arena UI (code/ui, MISSIONPACK) parses teaminfo.txt and gameinfo.txt
# from pk3 content, so the fixture includes the real ui_main.c.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/ui_teaminfo_regression.c" \
    "$Q3_TEST_ROOT/code/ui/ui_shared.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_teaminfo"

# One process per case: UI_HeadCountByTeam builds its team mask only once.
# 31 teams stay within the int mask, 32 reach 1 << 31, 33 and 64 reach
# 1 << 32 and up, and 70 also overfill teamList[MAX_TEAMS].
for Q3_TEST_CASE in lists "heads 31" "heads 32" "heads 33" "heads 64" "heads 70"; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    # shellcheck disable=SC2086 # the case is a mode and its argument
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/ui_teaminfo" $Q3_TEST_CASE
done
