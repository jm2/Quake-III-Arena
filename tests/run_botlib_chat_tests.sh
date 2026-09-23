#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-botlib-chat.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #246: also build with the Retro68 default unsigned char; unset (-1)
# match offsets must stay unset whatever the signedness of plain char.
for Q3_TEST_MODE in host unsigned-char; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == unsigned-char ]]; then Q3_TEST_FLAGS=(-funsigned-char); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/botlib_chat_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/vm.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/dispatcher"
    "${CC:-cc}" -DBOTLIB -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/botlib_chat_native_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/native"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    for binary in "$Q3_TEST_DIR/dispatcher" "$Q3_TEST_DIR/native"; do
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$binary"
    done
done
