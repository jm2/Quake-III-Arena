#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-zip-reopen.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
python3 - "$Q3_TEST_DIR/large.pk3" <<'PY_ZIP'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w", compression=zipfile.ZIP_DEFLATED) as archive:
    with archive.open("large.bin", "w") as entry:
        for _ in range(32):
            entry.write(bytes(1024 * 1024))
PY_ZIP
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/fs_zip_reopen_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -lz -o "$Q3_TEST_DIR/reopen-tests"
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/reopen-tests" "$Q3_TEST_DIR/large.pk3"
done
