#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIRECTORY="$(mktemp -d -p "$TMPDIR" q3-font-freetype.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIRECTORY"' EXIT
python3 - "$Q3_TEST_ROOT" "$Q3_TEST_DIRECTORY" <<'PYCODE'
from pathlib import Path
import re,sys
root,out=map(Path,sys.argv[1:])
source=(root/'code/renderer/tr_font.c').read_text()
source,count=re.subn(r'^#include "\.\./ft2/[^"\n]+"\n','',source,flags=re.M)
assert count==5,'Only replace unavailable legacy FT header imports; retain all native bodies'
(out/'tr_font.c').write_text(source)
PYCODE
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -I "$Q3_TEST_ROOT/code/renderer" \
    -DQ3_FONT_FREETYPE_SOURCE="\"$Q3_TEST_DIRECTORY/tr_font.c\"" \
    "$Q3_TEST_ROOT/tests/font_freetype_regression.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIRECTORY/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIRECTORY/test"
