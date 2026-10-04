#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-string-pool.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issues #49 and #467: the Team Arena UI (code/ui, MISSIONPACK) keeps its list
# names in ui_shared.c's string pool. Each fixture includes the real ui_main.c
# or ui_gameinfo.c, fills the pool, and checks that every list loader keeps only
# the entries whose strings were stored and that hash-colliding strings are
# still found.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/ui_string_pool_regression.c" \
    "$Q3_TEST_ROOT/code/ui/ui_shared.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_string_pool"
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -DMISSIONPACK \
    "$Q3_TEST_ROOT/tests/ui_string_pool_maps_regression.c" \
    "$Q3_TEST_ROOT/code/ui/ui_shared.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_string_pool_maps"

# One process per case: a full pool stays full.
run() {
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$1" "$2"
}
for Q3_TEST_CASE in chain room mods movies demos characters; do
    run ui_string_pool "$Q3_TEST_CASE"
done
for Q3_TEST_CASE in room names levelshot; do
    run ui_string_pool_maps "$Q3_TEST_CASE"
done
