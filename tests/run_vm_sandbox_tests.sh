#!/usr/bin/env bash
# Issue #35: QVM syscalls that re-enter or free the running interpreted VM
# (tests/vm_sandbox_regression.c, on a 1 MB thread like the Mac's stack),
# then a short deterministic fuzz pass of random and mutated QVMs through the
# real VM_Create, VM_Call and VM_Restart (tests/vm_sandbox_fuzz.c).
#
# Optional retail pass: with Q3_DEMO_PAK0 naming the Quake III demo's
# baseq3/pak0.pk3, its cgame, qagame and ui QVMs run their init entry points
# and a few frames in the interpreter, then mutated copies of them are
# fuzzed. The pk3 is only extracted into the scratch directory.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-vm-sandbox.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
C="$Q3_TEST_ROOT/code"

for test in regression fuzz; do
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined -fno-sanitize-recover=undefined -pthread \
        "$Q3_TEST_ROOT/tests/vm_sandbox_$test.c" \
        "$C/qcommon/vm_interpreted.c" "$C/qcommon/vm.c" "$C/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$test"
done

export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$Q3_TEST_DIR/regression"

F="$Q3_TEST_DIR/fuzz"
"$F" header 1 20000
"$F" program 1 4000
"$F" program-reenter 1 2000
"$F" program-free 1 2000

Q3_DEMO="${Q3_DEMO_PAK0:-}"
if [[ -n "$Q3_DEMO" ]]; then
    python3 - "$Q3_DEMO" "$Q3_TEST_DIR/pak" <<'PY_EXTRACT'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as pak:
    names = [n for n in pak.namelist()
             if n.lower().startswith(("vm/", "scripts/")) and not n.endswith("/")]
    if not {"vm/cgame.qvm", "vm/qagame.qvm", "vm/ui.qvm"} <= set(n.lower() for n in names):
        sys.exit("%s has no baseq3 QVMs" % sys.argv[1])
    for name in names:
        pak.extract(name, sys.argv[2])
PY_EXTRACT
    V="$Q3_TEST_DIR/pak/vm"
    VM_FUZZ_PAKDIR="$Q3_TEST_DIR/pak" "$F" retail 0 3 "$V/cgame.qvm" "$V/qagame.qvm" "$V/ui.qvm"
    "$F" mutate 1 1500 "$V/cgame.qvm" "$V/qagame.qvm" "$V/ui.qvm"
else
    echo "vm sandbox: SKIPPED the retail QVM pass (set Q3_DEMO_PAK0 to the demo's baseq3/pak0.pk3)"
fi
echo "vm sandbox: all passes completed"
