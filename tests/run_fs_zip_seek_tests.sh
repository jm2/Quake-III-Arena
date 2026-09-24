#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-fs-zip-seek.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
for Q3_TEST_MODE in normal fast; do
python3 - "$Q3_TEST_DIR" <<'PY_ZIP'
import sys,zipfile
from pathlib import Path
def pattern(i):
 x=((i+1)*2654435761)&0xffffffff;x^=x>>16;x=(x*2246822519)&0xffffffff;x^=x>>13;return x&255
for name,method in [("native.pk3",zipfile.ZIP_DEFLATED),("stored.pk3",zipfile.ZIP_STORED)]:
 with zipfile.ZipFile(Path(sys.argv[1])/name,"w",compression=method) as z:
  z.writestr("native.bin",bytes(pattern(i) for i in range(3*65536+17)))
  z.writestr("other.bin",bytes(pattern(i)^0xff for i in range(3*65536+17)))
PY_ZIP
 Q3_TEST_FLAGS=(); if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math); fi
 "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" "$Q3_TEST_ROOT/tests/fs_zip_seek_regression.c" "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/seek-tests"
 for Q3_ARCHIVE in native.pk3 stored.pk3; do
  for Q3_CASE in 0 1 2 3 4 5 6 7 8 9 10; do ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/seek-tests" "$Q3_TEST_DIR/$Q3_ARCHIVE" "$Q3_CASE"; done
 done
done
