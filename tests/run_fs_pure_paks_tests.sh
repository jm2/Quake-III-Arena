#!/usr/bin/env bash
# Issue #342: pure pak name lists longer than their checksum lists must not
# leak small-zone strings on every gamestate. The names live inside the zone
# arena, so the test checks the real zone's accounting, not LeakSanitizer.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-fs-pure-paks.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
# The real zone allocator and CopyString; the fixture supplies the engine
# error and print imports.
python3 - "$Q3_TEST_ROOT" "$Q3_TEST_DIR" <<'PY_SOURCE'
from pathlib import Path
import re, sys
root, out = map(Path, sys.argv[1:])
source = (root / 'code/qcommon/common.c').read_text()
for name in ('Com_Error', 'Com_Printf', 'Com_DPrintf', 'Z_LogHeap', 'Hunk_Log', 'Hunk_SmallLog'):
    source, count = re.subn(r'(?m)^(void(?: QDECL)? )' + name + r'(\s*\()', r'\1FixtureUnused_' + name + r'\2', source)
    assert count == 1, (name, count)
(out / 'common.c').write_text(source)
PY_SOURCE
# Zone blocks are 4-byte aligned, as on the ILP32 target; LP64 hosts would
# flag their 8-byte pointer members as misaligned.
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-null-dereference \
        -fsanitize=address,undefined -fno-sanitize=alignment "${Q3_TEST_FLAGS[@]}" \
        -I"$Q3_TEST_ROOT/code/qcommon" "-DQ3_PURE_COMMON_SOURCE=\"$Q3_TEST_DIR/common.c\"" \
        "$Q3_TEST_ROOT/tests/fs_pure_paks_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/cmd.c" "$Q3_TEST_ROOT/code/qcommon/md4.c" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/pure-paks-tests"
    ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/pure-paks-tests"
done
