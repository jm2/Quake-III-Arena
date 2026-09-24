#!/usr/bin/env bash
# Issue #408: Com_Error must bound module and server text to com_errorMessage.
# The real Com_Error, Com_Printf and zone (common.c) run with the real cvar.c,
# vm.c and the real cgame and game error paths (cl_cgame.c, sv_game.c).
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-com-error-bound.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Zone blocks are 4-byte aligned, as on the ILP32 target; LP64 hosts would
# flag their 8-byte pointer members as misaligned.
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-null-dereference \
        -fsanitize=address,undefined -fno-sanitize=alignment "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/com_error_bound_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/cvar.c" "$Q3_TEST_ROOT/code/qcommon/vm.c" \
        "$Q3_TEST_ROOT/code/client/cl_cgame.c" "$Q3_TEST_ROOT/code/server/sv_game.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/com-error-bound-tests"
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/com-error-bound-tests"
done
