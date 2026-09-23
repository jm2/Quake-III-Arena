#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-server-userinfo-ip.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Pattern-fill uninitialized locals so a read of one (G_FilterPacket("")) is
# deterministic instead of depending on stale stack contents.
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -ftrivial-auto-var-init=pattern \
    "$Q3_TEST_ROOT/tests/server_userinfo_ip_regression.c" \
    "$Q3_TEST_ROOT/code/game/g_svcmds.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/server_userinfo_ip_regression"
# LeakSanitizer cannot initialize in the local ptrace sandbox.
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/server_userinfo_ip_regression"
