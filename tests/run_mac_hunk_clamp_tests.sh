#!/usr/bin/env bash
# Issue #230: Com_InitHunkMemory on the classic Mac clamps an oversized
# com_hunkMegs to what MaxBlock() says fits, never below the 56 MB floor, and
# stops with a clean Sys_Error when not even the floor fits.  The real
# common.c is built for __MACOS__ against a fake Memory Manager.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-mac-hunk-clamp.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# The fixture provides Com_Error, Com_Printf and the hunk logs.
python3 - "$Q3_TEST_ROOT/code/qcommon/common.c" "$Q3_TEST_DIR/common.c" <<'PY_SOURCE'
import re, sys
from pathlib import Path
source = Path(sys.argv[1]).read_text()
for name in ("Com_Error", "Com_Printf", "Hunk_Log", "Hunk_SmallLog"):
    source, count = re.subn(r"(?m)^(void(?: QDECL)? )" + name + r"(\s*\()", r"\1FixtureUnused_" + name + r"\2", source)
    assert count == 1, (name, count)
# The PPC32 cacheline alignment casts the pointer to int; widen it on a 64-bit host.
old = "s_hunkData = (byte *) ( ( (int)s_hunkData + 31 ) & ~31 );"
assert source.count(old) == 1
source = source.replace(old, "s_hunkData = (byte *) ( ( (intptr_t)s_hunkData + 31 ) & ~31 );")
Path(sys.argv[2]).write_text(source)
PY_SOURCE
mkdir -p "$Q3_TEST_DIR/include"
: > "$Q3_TEST_DIR/include/MacTypes.h"
printf 'long MaxBlock( void );\n' > "$Q3_TEST_DIR/include/MacMemory.h"

"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -U__linux__ -D__MACOS__ -Dcalloc=FakeCalloc \
    -I"$Q3_TEST_DIR/include" -I"$Q3_TEST_ROOT/code/qcommon" \
    -DQ3_HUNK_COMMON_SOURCE=\""$Q3_TEST_DIR/common.c"\" \
    "$Q3_TEST_ROOT/tests/mac_hunk_clamp_regression.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/mac_hunk_clamp"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/mac_hunk_clamp"
