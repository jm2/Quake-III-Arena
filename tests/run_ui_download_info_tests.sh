#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-download-info.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #429: the Team Arena UI (code/ui, MISSIONPACK) and the base q3_ui are
# linked natively, so the fixture includes the real ui_main.c or ui_connect.c
# and draws its download display from the download cvars. UBSan checks every
# division by zero, signed overflow and float to int conversion out of range,
# and stops at the first.
for Q3_TEST_CASE in "ui -DMISSIONPACK" "q3_ui"; do
    # shellcheck disable=SC2086 # the case is a UI and its flags
    set -- $Q3_TEST_CASE
    Q3_TEST_UI="$1"
    shift
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined,integer-divide-by-zero,signed-integer-overflow,float-cast-overflow \
        -fno-sanitize-recover=all "$@" \
        "$Q3_TEST_ROOT/tests/ui_download_info_regression.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_UI"

    # One process per case: UBSan stops at the first undefined operation.
    Q3_TEST_CASES="$("$Q3_TEST_DIR/$Q3_TEST_UI" count)"
    for (( Q3_TEST_INDEX = 0; Q3_TEST_INDEX < Q3_TEST_CASES; Q3_TEST_INDEX++ )); do
        # LeakSanitizer cannot initialize in the local ptrace sandbox.
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_DIR/$Q3_TEST_UI" "$Q3_TEST_INDEX"
    done
    echo "$Q3_TEST_UI download display draws $Q3_TEST_CASES download states without undefined arithmetic (issue #429)"
done
