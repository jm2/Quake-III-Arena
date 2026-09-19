#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-network-challenge.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then
        Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math)
    fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -Wno-pointer-to-int-cast \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/network_challenge_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" -Wl,--gc-sections -lm \
        -o "$Q3_TEST_DIR/network-challenge-tests"
    for Q3_CASE in {0..8}; do
        ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_DIR/network-challenge-tests" "$Q3_CASE"
    done

    "${CC:-cc}" -std=gnu99 -ffunction-sections -fdata-sections \
        "${Q3_TEST_FLAGS[@]}" -c "$Q3_TEST_ROOT/code/server/sv_client.c" \
        -o "$Q3_TEST_DIR/sv-client.o"
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/network_handshake_regression.c" \
        "$Q3_TEST_DIR/sv-client.o" -Wl,--gc-sections \
        -o "$Q3_TEST_DIR/network-handshake-tests"
    for Q3_CASE in 0 1; do
        ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_DIR/network-handshake-tests" "$Q3_CASE"
    done
done
