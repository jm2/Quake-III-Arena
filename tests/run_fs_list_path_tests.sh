#!/usr/bin/env bash
# Issue #384: listing "" (Load Config, UI_LoadTeams, fdir) must not read the
# byte before the caller's path, and short list paths list as before.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-fs-list-path.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
# Configs at the root as in pak0.pk3 (default.cfg), plus nested entries.
python3 - "$Q3_TEST_DIR/pak0.pk3" <<'PY_ZIP'
import sys, zipfile
entries = [
    ("ares.cfg", b"// ares\n"),
    ("default.cfg", b"// default\n"),
    ("scripts/base_wall.shader", b"shader\n"),
    ("levelshots/q3dm1.jpg", b"shot\n"),
    ("maps/q3dm1.bsp", b"bsp\n"),
    ("models/players/sarge/head.md3", b"head\n"),
    ("botfiles/bots/sarge_c.c", b"bot\n"),
]
with zipfile.ZipFile(sys.argv[1], "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for name, data in entries:
        archive.writestr(name, data)
PY_ZIP
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined "$Q3_TEST_ROOT/tests/fs_list_path_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/fs-list-path-tests"
ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/fs-list-path-tests" "$Q3_TEST_DIR/pak0.pk3" "$Q3_TEST_DIR"
