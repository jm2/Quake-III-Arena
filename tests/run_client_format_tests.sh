#!/usr/bin/env bash
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-client-format.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

# Issue #394: each test declares the engine's print functions with printf
# format checking before it includes the real client source, and builds with
# -Werror=format, so a format whose arguments do not match fails the build.
# The tests then compare what the commands print or queue at run time.
#   snd_console_regression: snd_dma.c's s_show 2, play and soundinfo (links
#                           the real cmd.c to tokenize the commands)
#   key_up_regression:      cl_keys.c's key-up button commands
for Q3_TEST_FIXTURE in snd_console_regression key_up_regression; do
    Q3_TEST_SOURCES=("$Q3_TEST_ROOT/tests/$Q3_TEST_FIXTURE.c" "$Q3_TEST_ROOT/code/game/q_shared.c")
    if [[ "$Q3_TEST_FIXTURE" == snd_console_regression ]]; then
        Q3_TEST_SOURCES+=("$Q3_TEST_ROOT/code/qcommon/cmd.c")
    fi
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -Wformat -Werror=format \
        -fsanitize=address,undefined -fno-sanitize-recover=all \
        "${Q3_TEST_SOURCES[@]}" -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
done
