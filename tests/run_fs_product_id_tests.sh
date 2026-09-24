#!/usr/bin/env bash
# Issue #426: the real FS_SetRestrictions must decode the full game's
# productid.txt with Q_rand's LCG without a signed overflow (no UBSan
# recover), accept it, and still reject a changed first or last byte.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-fs-product-id.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
# The full game's productid.txt, as the comment above fs_scrambledProductId
# in files.c spells it, and two copies with one byte changed.
python3 - "$Q3_TEST_DIR" <<'PY_ZIP'
import sys, zipfile
text = (b"This file is copyright 1999 Id Software, and may not be duplicated except "
        b"during a licensed installation of the full commercial version of Quake 3:Arena")
assert len(text) == 152
def pack(name, data):
    with zipfile.ZipFile(sys.argv[1] + "/" + name, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("default.cfg", b"// default\n")
        archive.writestr("productid.txt", data)
pack("retail.pk3", text)
pack("badfirst.pk3", b"t" + text[1:])
pack("badlast.pk3", text[:-1] + b"A")
PY_ZIP
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined,signed-integer-overflow -fno-sanitize-recover=all \
        "${Q3_TEST_FLAGS[@]}" "$Q3_TEST_ROOT/tests/fs_product_id_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/product-id-tests"
    ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/product-id-tests" "$Q3_TEST_DIR"
done
