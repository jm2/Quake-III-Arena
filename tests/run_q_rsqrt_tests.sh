#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-q-rsqrt.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Q_rsqrt's and SquareRootFloat's float bits must be read as 32 bits on every
# host (see the test). cm_trace.c needs the collision map, so the fixture takes
# SquareRootFloat from it verbatim, with a #line so reports name that file.
awk -v file="$Q3_TEST_ROOT/code/qcommon/cm_trace.c" '
    /^float[[:space:]]+SquareRootFloat[[:space:]]*[(]/ { printf "#line %d \"%s\"\n", NR, file; found = 1 }
    found { print }
    found && /^}/ { found = 0; count++ }
    END { exit count == 1 ? 0 : 1 }
' "$Q3_TEST_ROOT/code/qcommon/cm_trace.c" > "$Q3_TEST_DIR/square_root_float_extracted.c" ||
    { echo "run_q_rsqrt_tests: no single SquareRootFloat in cm_trace.c" >&2; exit 1; }
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -fno-strict-aliasing -fsanitize=address,undefined \
    -I"$Q3_TEST_DIR" "$Q3_TEST_ROOT/tests/q_rsqrt_regression.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" -lm -o "$Q3_TEST_DIR/q_rsqrt"
ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/q_rsqrt"
