#!/usr/bin/env bash
# Issue #230: the SIZE minimum in code/mac/mac_resources.r must hold the
# engine's fixed demand (hunk, zone, small zone, sound pool, stack and image)
# with headroom, and Com_InitHunkMemory's MaxBlock() reserve must agree with
# that budget. cmake/mac_partition.py reads the Mac defaults from the sources;
# a larger default, or a smaller SIZE, fails here. The Retro68 build runs the
# same check on the link map, so a larger image fails that build.
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_TOOL="$Q3_TEST_ROOT/cmake/mac_partition.py"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-mac-partition.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

python3 "$Q3_TEST_TOOL" host --root "$Q3_TEST_ROOT"

# A copy of the files the check reads, for the cases it must refuse.
copy_tree() {
    rm -rf "$Q3_TEST_DIR/root"
    for f in code/qcommon/common.c code/client/snd_mem.c code/client/snd_local.h code/mac/mac_resources.r; do
        mkdir -p "$Q3_TEST_DIR/root/$(dirname "$f")"
        cp "$Q3_TEST_ROOT/$f" "$Q3_TEST_DIR/root/$f"
    done
}
expect_refusal() {
    if python3 "$Q3_TEST_TOOL" "$@" --root "$Q3_TEST_DIR/root" > "$Q3_TEST_DIR/out" 2>&1; then
        cat "$Q3_TEST_DIR/out" >&2
        echo "mac partition check accepted: $Q3_TEST_CASE" >&2
        exit 1
    fi
}

Q3_TEST_CASE="the 96,000 KiB minimum before issue #230"
copy_tree
sed -i 's/^\t120000 \* 1024$/\t96000 * 1024/' "$Q3_TEST_DIR/root/code/mac/mac_resources.r"
grep -q '^	96000 \* 1024$' "$Q3_TEST_DIR/root/code/mac/mac_resources.r"
expect_refusal host

Q3_TEST_CASE="retail's com_soundMegs 8 on the Mac"
copy_tree
python3 - "$Q3_TEST_DIR/root/code/client/snd_mem.c" <<'PY'
import sys
path = sys.argv[1]
text = open(path).read()
assert text.count('#define DEF_COMSOUNDMEGS "4"') == 1
open(path, "w").write(text.replace('#define DEF_COMSOUNDMEGS "4"', '#define DEF_COMSOUNDMEGS "8"'))
PY
expect_refusal host

Q3_TEST_CASE="a 64 MB hunk default on the Mac"
copy_tree
python3 - "$Q3_TEST_DIR/root/code/qcommon/common.c" <<'PY'
import sys
path = sys.argv[1]
text = open(path, newline="").read()
old = '#else\n#define DEF_COMHUNKMEGS "56"'
assert text.replace("\r\n", "\n").count(old) == 1
text = text.replace(old, '#else\n#define DEF_COMHUNKMEGS "64"').replace(old.replace("\n", "\r\n"), '#else\r\n#define DEF_COMHUNKMEGS "64"')
open(path, "w", newline="").write(text)
PY
expect_refusal host

# The hunk clamp's reserve: too small for the sound pool and headroom, or so
# large that the default hunk no longer fits beside it at the minimum.
set_reserve() {
    python3 - "$Q3_TEST_DIR/root/code/qcommon/common.c" "$1" <<'PY'
import sys
path, value = sys.argv[1:]
text = open(path, newline="").read()
old = "#define MAC_HUNK_RESERVE_KB\t( 12368 + 8 * 1024 )"
assert text.count(old) == 1
open(path, "w", newline="").write(text.replace(old, "#define MAC_HUNK_RESERVE_KB\t" + value))
PY
}
Q3_TEST_CASE="a hunk reserve without the headroom"
copy_tree
set_reserve "( 12368 + 4 * 1024 )"
expect_refusal host
Q3_TEST_CASE="a hunk reserve that clamps the default hunk at the minimum"
copy_tree
set_reserve "( 12368 + 24 * 1024 )"
expect_refusal host
Q3_TEST_CASE="no hunk reserve"
copy_tree
sed -i 's/^#define MAC_HUNK_RESERVE_KB/#define MAC_HUNK_RESERVE/' "$Q3_TEST_DIR/root/code/qcommon/common.c"
command grep -q '^#define MAC_HUNK_RESERVE	' "$Q3_TEST_DIR/root/code/qcommon/common.c"
expect_refusal host

# The link-map check: an image within the allowance passes, a larger one fails.
write_map() {
    printf '.text           0x0000000000000000   0x%x\n' "$1"
    printf '.data           0x0000000000000000    0x40000\n'
    printf '                0x0000000000000000                q3static_game_data_start = .\n'
    printf '                0x0000000000001000                q3static_game_data_end = .\n'
    printf '                0x0000000000001000                q3static_cgame_data_start = .\n'
    printf '                0x0000000000002000                q3static_cgame_data_end = .\n'
    printf '                0x0000000000002000                q3static_ui_data_start = .\n'
    printf '                0x0000000000003000                q3static_ui_data_end = .\n'
    printf '.bss            0x0000000000040000  0x1000000\n'
}
copy_tree
write_map $((0x380000)) > "$Q3_TEST_DIR/fits.map"
python3 "$Q3_TEST_TOOL" map --root "$Q3_TEST_DIR/root" --map "$Q3_TEST_DIR/fits.map" > /dev/null
Q3_TEST_CASE="an image over the allowance"
write_map $((0x800000)) > "$Q3_TEST_DIR/large.map"
expect_refusal map --map "$Q3_TEST_DIR/large.map"
Q3_TEST_CASE="a map without the module brackets"
write_map $((0x380000)) | command grep -v q3static_ui > "$Q3_TEST_DIR/brackets.map"
expect_refusal map --map "$Q3_TEST_DIR/brackets.map"

echo "The SIZE minimum holds the fixed demand with headroom, and the check refuses less (issue #230)"
