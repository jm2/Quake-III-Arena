#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_BINARY="$(mktemp "${TMPDIR:-/var/tmp}/q3-hunk-allocation.XXXXXX")"
Q3_TEST_SOURCE="$(mktemp "${TMPDIR:-/var/tmp}/q3-hunk-source.XXXXXX")"
trap 'rm -f -- "$Q3_TEST_BINARY" "$Q3_TEST_SOURCE"' EXIT
# Redirect error/log implementations only; allocator code and state stay native.
python3 - "$Q3_TEST_ROOT/code/qcommon/common.c" "$Q3_TEST_SOURCE" <<'PY_SOURCE'
import re, sys
from pathlib import Path
source = Path(sys.argv[1]).read_text()
for name in ("Com_Error", "Hunk_Log", "Hunk_SmallLog"):
    source, count = re.subn(r"(?m)^(void(?: QDECL)? )" + name + r"(\s*\()", r"\1FixtureUnused_" + name + r"\2", source)
    assert count == 1, (name, count)
Path(sys.argv[2]).write_text(source)
PY_SOURCE
for Q3_TEST_MODE in release debug; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == debug ]]; then Q3_TEST_FLAGS=(-DHUNK_DEBUG); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined -I"$Q3_TEST_ROOT/code/qcommon" "${Q3_TEST_FLAGS[@]}" -DQ3_HUNK_COMMON_SOURCE=\""$Q3_TEST_SOURCE"\" \
        "$Q3_TEST_ROOT/tests/hunk_allocation_regression.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_BINARY"
done
