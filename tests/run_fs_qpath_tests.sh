#!/usr/bin/env bash
# Issue #262: qpaths and fs_game values with ':' or (on HFS) doubled
# separators must never reach an OS path outside the game directory.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-fs-qpath.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
python3 - "$Q3_TEST_DIR/retail.pk3" <<'PY_ZIP'
import sys, zipfile
entries = {
    "models/players/sarge/head.md3": b"head\n",
    "sound/player/sarge/death1.wav": b"death\n",
    "scripts/base_wall.shader": b"shader\n",
    "textures/base_wall/bluemetal1light.tga": b"tga\n",
    "maps/q3dm1.bsp": b"bsp\n",
    "botfiles/bots/sarge_c.c": b"bot\n",
    "levelshots/q3dm1.jpg": b"shot\n",
    "video/intro.roq": b"roq\n",
}
with zipfile.ZipFile(sys.argv[1], "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for name, data in entries.items():
        archive.writestr(name, data)
PY_ZIP
for Q3_TEST_SEP in hfs host; do
    for Q3_TEST_MODE in normal fast; do
        Q3_TEST_FLAGS=()
        if [[ "$Q3_TEST_SEP" == hfs ]]; then Q3_TEST_FLAGS+=(-DQ3_TEST_HFS); fi
        if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS+=(-O2 -DNDEBUG -ffast-math); fi
        Q3_TEST_WORK="$Q3_TEST_DIR/$Q3_TEST_SEP-$Q3_TEST_MODE"
        mkdir "$Q3_TEST_WORK"
        "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
            -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
            "$Q3_TEST_ROOT/tests/fs_qpath_regression.c" \
            "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
            -Wl,--gc-sections -lm -lz -o "$Q3_TEST_WORK/qpath-tests"
        ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
            UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_WORK/qpath-tests" "$Q3_TEST_DIR/retail.pk3" "$Q3_TEST_WORK"
    done
done
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined "$Q3_TEST_ROOT/tests/cl_fs_game_regression.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/fs-game-tests"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/fs-game-tests"
