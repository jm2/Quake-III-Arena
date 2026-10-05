#!/usr/bin/env bash
# Startup idlogo/intro cinematics play through as in retail 1.32c (#14).
# Set Q3_CIN_PK3 to a real pk3 holding video/idlogo.RoQ (for example the demo
# pak0.pk3) to also play the retail movie; game data is never part of the
# repository.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-cin-startup.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
python3 "$Q3_TEST_ROOT/tests/create_cin_startup_fixtures.py" "$Q3_TEST_DIR"
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined "$Q3_TEST_ROOT/tests/cin_startup_regression.c" \
    "$Q3_TEST_ROOT/tests/cin_startup_fs.c" \
    "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/cin-startup-tests"
run() {
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/cin-startup-tests" "$@"
}
run "$Q3_TEST_DIR/cin.pk3"
if [[ -n "${Q3_CIN_PK3:-}" ]]; then
    run "$Q3_CIN_PK3" real
fi
