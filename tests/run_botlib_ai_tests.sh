#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-botlib-ai.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined \
    "$Q3_TEST_ROOT/tests/botlib_ai_regression.c" "$Q3_TEST_ROOT/code/qcommon/vm.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/dispatcher"
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined "$Q3_TEST_ROOT/tests/botlib_genetic_regression.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/genetic"

# LeakSanitizer cannot initialize in the local ptrace sandbox.
for binary in "$Q3_TEST_DIR/dispatcher" "$Q3_TEST_DIR/genetic"; do
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$binary"
done
