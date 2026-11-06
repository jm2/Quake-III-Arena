#!/usr/bin/env bash
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-unzip-tree.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
Q3_TEST_BINARY="$Q3_TEST_DIR/test"

# A real dynamic-Huffman block, which a stored or fixed block would not be.
python3 - "$Q3_TEST_DIR/good_tree.h" <<'PY_SOURCE'
import sys,zlib
text = b"".join(b"%d quake three arena %x\n" % (i, i * 2654435761 % 65521) for i in range(200))
packer = zlib.compressobj(9, zlib.DEFLATED, -15)
data = packer.compress(text) + packer.flush()
assert (data[0] >> 1) & 3 == 2, "not a dynamic block"
with open(sys.argv[1], "w") as out:
    out.write("static const byte goodTree[] = {" + ",".join(map(str, data)) + "};\n")
PY_SOURCE

"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    "-DQ3_UNZIP_GOOD_TREE=\"$Q3_TEST_DIR/good_tree.h\"" \
    "$Q3_TEST_ROOT/tests/unzip_tree_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_BINARY"

ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BINARY"
