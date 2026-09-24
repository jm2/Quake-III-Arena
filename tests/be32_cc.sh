#!/usr/bin/env bash
# Compiler wrapper for running the host regressions on 32-bit big-endian
# PowerPC Linux under qemu-user (the "Host C regressions (ppc32 BE)" CI job):
#
#   CC="$PWD/tests/be32_cc.sh" QEMU_LD_PREFIX=/usr/powerpc-linux-gnu \
#   Q3_TEST_DETECT_LEAKS=0 bash tests/run_host_regressions.sh \
#       --skip-file tests/be32_skip.txt
#
# The runners keep their own flags, sanitizers included. This wrapper swaps in
# the cross compiler ($Q3_BE32_CC, default powerpc-linux-gnu-gcc) and adds:
#   -fsigned-char, -mlong-double-64  the Retro68 target's char and long double;
#   -latomic                         the ppc32 sanitizer runtimes' 64-bit atomics.
# The binaries run through a qemu-ppc binfmt_misc handler. QEMU_LD_PREFIX
# points qemu at the cross sysroot, and ppc32 has no LeakSanitizer, so leak
# checking runners must see Q3_TEST_DETECT_LEAKS=0.
set -euo pipefail

extra=()
link=1
for arg in "$@"; do
    case "$arg" in
        -c|-S|-E|-M|-MM) link=0 ;;
    esac
done
if [ "$link" -eq 1 ]; then
    extra+=(-latomic)
fi

exec "${Q3_BE32_CC:-powerpc-linux-gnu-gcc}" -fsigned-char -mlong-double-64 "$@" "${extra[@]}"
