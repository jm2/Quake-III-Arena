#!/usr/bin/env bash
# Compiler wrapper that tests/run_host_regressions.sh puts in front of $CC
# (issue #395), so every runner builds the engine and module sources it uses
# with the format checks of the Retro68 build (CMakeLists.txt):
#
#   Q3_FORMAT_CC=gcc CC="$PWD/tests/format_cc.sh" bash tests/run_<name>_tests.sh
#
# The flags come first, so a runner's own -Wno-format... still wins.
set -euo pipefail

exec "${Q3_FORMAT_CC:?Q3_FORMAT_CC names the real compiler}" \
    -Wformat=2 -Wno-format-nonliteral -Werror=format -Werror=format-security \
    -Wno-error=format-overflow -Wno-error=format-truncation "$@"
