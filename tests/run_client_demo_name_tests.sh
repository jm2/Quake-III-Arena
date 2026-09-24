#!/usr/bin/env bash
# Issue #384: "demo <name>" must not point before a name shorter than six
# characters, and demo lookup must stay as in 1.32c.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-client-demo-name.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY"' EXIT

# pointer-overflow is part of undefined; it is named for the test's purpose.
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined,pointer-overflow \
    "$Q3_TEST_ROOT/tests/client_demo_name_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

# LeakSanitizer cannot initialize in the local ptrace sandbox.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
