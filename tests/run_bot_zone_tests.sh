#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_SOURCE="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-bot-zone-source.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_SOURCE"' EXIT
python3 - "$Q3_TEST_ROOT" "$Q3_TEST_SOURCE" <<'PY_SOURCE'
from pathlib import Path
import re, sys
root, out = map(Path, sys.argv[1:])
source = (root / 'code/qcommon/common.c').read_text()
for name in ('Com_Error', 'Com_Memset', 'Z_LogHeap', 'Hunk_Log', 'Hunk_SmallLog'):
    source, count = re.subn(r'(?m)^(void(?: QDECL)? )' + name + r'(\s*\()', r'\1FixtureUnused_' + name + r'\2', source)
    assert count >= 1, (name, count)
(out / 'common.c').write_text(source)
source = (root / 'code/server/sv_bot.c').read_text()
parts = []
for name in ('BotImport_GetMemory', 'BotImport_HunkAlloc'):
    start = 'void *' + name + '('
    assert source.count(start) == 1, ('native import seam changed', name)
    first = source.index(start)
    last = source.index('\n}', first) + 2
    parts.append(source[first:last])
(out / 'imports.h').write_text('\n'.join(parts) + '\n')
PY_SOURCE
for Q3_TEST_MODE in release debug; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == debug ]]; then Q3_TEST_FLAGS=(-DZONE_DEBUG -DHUNK_DEBUG); fi
    for Q3_TEST_OPTIMIZATION in normal optimized; do
        Q3_TEST_OPT_FLAGS=()
        if [[ "$Q3_TEST_OPTIMIZATION" == optimized ]]; then Q3_TEST_OPT_FLAGS=(-O2 -DNDEBUG); fi
        "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
            -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" "${Q3_TEST_OPT_FLAGS[@]}" \
            -I"$Q3_TEST_ROOT/code/qcommon" "-DQ3_ZONE_COMMON_SOURCE=\"$Q3_TEST_SOURCE/common.c\"" \
            "-DQ3_ZONE_IMPORT_SOURCE=\"$Q3_TEST_SOURCE/imports.h\"" \
            "$Q3_TEST_ROOT/tests/bot_zone_regression.c" -Wl,--gc-sections -lm -o "$Q3_TEST_SOURCE/test"
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_SOURCE/test"
    done
done
