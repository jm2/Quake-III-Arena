#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-q3ui-color1.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# The base q3_ui is linked natively; the fixture includes the real
# ui_playersettings.c and runs it behind the real CL_SystemInfoChanged, cvar.c
# and the module's syscall stubs (tests/ui_cvar_syscalls.h), as the
# cvar index fixtures do.
Q3_TEST_Q3UI=()
for Q3_TEST_SOURCE in "$Q3_TEST_ROOT"/code/q3_ui/ui_*.c; do
    # the fixture includes ui_playersettings.c; CMake leaves the ranking menus out
    case "$Q3_TEST_SOURCE" in
        */ui_playersettings.c|*/ui_rankings.c|*/ui_rankstatus.c|*/ui_signup.c|*/ui_login.c|*/ui_specifyleague.c) ;;
        *) Q3_TEST_Q3UI+=("$Q3_TEST_SOURCE") ;;
    esac
done
"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined,float-cast-overflow \
    "$Q3_TEST_ROOT/tests/q3ui_color1_regression.c" "${Q3_TEST_Q3UI[@]}" \
    "$Q3_TEST_ROOT/code/game/bg_misc.c" "$Q3_TEST_ROOT/tests/systeminfo_cvar_harness.c" \
    "$Q3_TEST_ROOT/code/qcommon/cvar.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/q3ui_color1"

# color1 and the game color CG_ColorFromString draws for it: atoi's value from
# 1 to 7, else white (7). The seven colors are as in retail; the other strings
# are ones a float reading shows differently.
for Q3_TEST_CASE in "1 1" "2 2" "3 3" "4 4" "5 5" "6 6" "7 7" "0 7" "8 7" "-1 7" \
    "1e1 1" "4e1 4" "0.5 7" "0x3 7" "7.9 7" "2147483647 7" "-2147483648 7"; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    # shellcheck disable=SC2086 # the case is a color1 string and its game color
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/q3ui_color1" $Q3_TEST_CASE
done
