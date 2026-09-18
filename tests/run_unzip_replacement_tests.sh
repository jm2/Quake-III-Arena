#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-unzip-replacement.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
python3 - "$Q3_TEST_DIR" <<'PY_ZIP'
import sys, zipfile
from pathlib import Path
for name, method in [("native.pk3", zipfile.ZIP_DEFLATED), ("stored.pk3", zipfile.ZIP_STORED)]:
    with zipfile.ZipFile(Path(sys.argv[1]) / name, "w", compression=method) as archive:
        archive.writestr("native.bin", bytes((i * 73 + 19) & 255 for i in range(3 * 65536 + 17)))
PY_ZIP
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/unzip_replacement_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -lz -o "$Q3_TEST_DIR/replacement-tests"
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/replacement-tests" "$Q3_TEST_DIR/native.pk3" "$Q3_TEST_DIR/stored.pk3" all
done
