#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-unzip-metadata.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
for Q3_TEST_MODE in normal fast; do
python3 - "$Q3_TEST_DIR" <<'PY_ZIP'
import sys, zipfile
from pathlib import Path
for name, method in [("native.pk3", zipfile.ZIP_DEFLATED), ("stored.pk3", zipfile.ZIP_STORED)]:
    path = Path(sys.argv[1]) / name
    with zipfile.ZipFile(path, "w", compression=method) as archive:
        entry = zipfile.ZipInfo("native.txt")
        entry.compress_type = method
        entry.extra = b"\xfe\xca\x10\0" + b"0123456789abcdef"
        archive.writestr(entry, b"native data\n")
        archive.comment = b"native comment!"
    (Path(sys.argv[1]) / ("prefix-" + name)).write_bytes(b"SFX" + bytes(range(20)) + path.read_bytes())
PY_ZIP
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/unzip_optional_metadata_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/metadata-tests"
    ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/metadata-tests" "$Q3_TEST_DIR/native.pk3" "$Q3_TEST_DIR/stored.pk3" "$Q3_TEST_DIR/prefix-native.pk3" "$Q3_TEST_DIR/prefix-stored.pk3"
done
