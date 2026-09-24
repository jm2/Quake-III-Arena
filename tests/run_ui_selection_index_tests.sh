#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-selection-index.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #419: the Team Arena UI (code/ui, MISSIONPACK) is linked natively, so
# the fixture includes the real ui_main.c for its static menu scripts and feeder
# selection, links the rest of the UI with its real syscall layer, and answers
# the syscalls as the engine does.
Q3_TEST_UI=()
for Q3_TEST_SOURCE in "$Q3_TEST_ROOT"/code/ui/ui_*.c; do
    if [[ "$Q3_TEST_SOURCE" != */ui_main.c ]]; then
        Q3_TEST_UI+=("$Q3_TEST_SOURCE")
    fi
done
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/ui_selection_index_regression.c" "${Q3_TEST_UI[@]}" \
    "$Q3_TEST_ROOT/code/game/bg_misc.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_selection_index"

# One process per case: UBSan and ASan stop at the first read past a table.
# LeakSanitizer cannot initialize in the local ptrace sandbox.
Q3_TEST_RUN=(env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1)
# addBot with each gametype's table: 3 team heads or a full MAX_HEADS (64), and
# the fixture's 3 bots. The heads item steps botIndex to two past its table
# (heads + 1), and the bots item to two past the bots (up to MAX_BOTS + 1).
for Q3_TEST_GAMETYPE in 0 1 2 3 4 5 6 7; do
    for Q3_TEST_HEADS in 3 64; do
        for Q3_TEST_INDEX in 0 2 3 4 63 64 65 1025 -1 2147483647 -2147483648; do
            "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui_selection_index" addbot \
                "$Q3_TEST_GAMETYPE" "$Q3_TEST_HEADS" "$Q3_TEST_INDEX"
        done
    done
done
# Server lists of 0 rows, 3 rows and a full MAX_DISPLAY_SERVERS (2048). An empty
# list selects row -1 and a click below the last row selects the row count.
for Q3_TEST_ROWS in 0 3 2048; do
    for Q3_TEST_ROW in 0 1 $((Q3_TEST_ROWS - 1)) "$Q3_TEST_ROWS" -1 2147483647 -2147483648; do
        "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui_selection_index" select "$Q3_TEST_ROWS" "$Q3_TEST_ROW"
        for Q3_TEST_SCRIPT in ServerStatus addFavorite deleteFavorite JoinServer; do
            "${Q3_TEST_RUN[@]}" "$Q3_TEST_DIR/ui_selection_index" script \
                "$Q3_TEST_SCRIPT" "$Q3_TEST_ROWS" "$Q3_TEST_ROW"
        done
    done
done
