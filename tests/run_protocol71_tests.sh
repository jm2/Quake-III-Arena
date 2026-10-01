#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-protocol71.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
    -Wno-pointer-to-int-cast -fsanitize=address,undefined)

# Issue #37: the real client (cl_main.c, included by the fixture, and
# cl_net_chan.c) and server (sv_main.c, sv_net_chan.c and sv_client.c,
# included by protocol71_server.c) with the real netchan, message, Huffman
# and command-token code, talking to each other and to models of retail
# 1.32c, ioquake3 and Quake3e peers.
"${CC:-cc}" "${Q3_TEST_FLAGS[@]}" \
    "$Q3_TEST_ROOT/tests/protocol71_regression.c" "$Q3_TEST_ROOT/tests/protocol71_server.c" \
    "$Q3_TEST_ROOT/code/client/cl_net_chan.c" "$Q3_TEST_ROOT/code/server/sv_main.c" \
    "$Q3_TEST_ROOT/code/server/sv_net_chan.c" "$Q3_TEST_ROOT/code/qcommon/net_chan.c" \
    "$Q3_TEST_ROOT/code/qcommon/msg.c" "$Q3_TEST_ROOT/code/qcommon/huffman.c" \
    "$Q3_TEST_ROOT/code/qcommon/cmd.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/protocol71-tests"
# 0 ioquake3's packets, 1 a retail client, 2 a retail server, 3 protocol 71,
# 4 ioquake3 and Quake3e servers, 5 spoofed setup and timeouts, 6 spoofed
# sequenced packets, 7 com_protocol 68, 8 starved getchallenges and the
# challenge generator, 9 a recycled challenge record.
for Q3_CASE in 0 1 2 3 4 5 6 7 8 9; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/protocol71-tests" "$Q3_CASE"
done
