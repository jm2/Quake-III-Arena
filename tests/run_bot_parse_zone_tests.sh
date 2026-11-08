#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-bot-parse-zone.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# The botlib parser's file-driven allocations must fail the parse, not the
# game, when the zone has no block that fits (see the test).
"${CC:-cc}" -DBOTLIB -std=gnu99 -fgnu89-inline -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/bot_parse_zone_regression.c" "$Q3_TEST_ROOT/code/botlib/l_memory.c" \
    "$Q3_TEST_ROOT/code/botlib/l_precomp.c" "$Q3_TEST_ROOT/code/botlib/l_script.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/bot_parse_zone"
ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/bot_parse_zone"
