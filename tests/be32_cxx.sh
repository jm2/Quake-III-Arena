#!/usr/bin/env bash
# The C++ counterpart of tests/be32_cc.sh, for runners that compile C++ (such
# as run_mac_toolbox_init_tests.sh, which builds code/mac/mac_consolehooks.cc).
# Runners pick it when $CC is be32_cc.sh. It swaps in the cross compiler
# ($Q3_BE32_CXX, default powerpc-linux-gnu-g++) and adds the same flags:
#   -fsigned-char, -mlong-double-64  the Retro68 target's char and long double;
#   -latomic                         the ppc32 sanitizer runtimes' 64-bit atomics.
set -euo pipefail

extra=()
link=1
for arg in "$@"; do
    case "$arg" in
        -c|-S|-E|-M|-MM|-dumpmachine) link=0 ;;
    esac
done
if [ "$link" -eq 1 ]; then
    extra+=(-latomic)
fi

exec "${Q3_BE32_CXX:-powerpc-linux-gnu-g++}" -fsigned-char -mlong-double-64 "$@" "${extra[@]}"
