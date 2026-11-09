#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-setcolor.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# RB_SetColor and RB_ColorByte alone: tr_backend.c needs the whole renderer.
Q3_TEST_EXTRACT() {
    awk -v start="$1" -v file="$2" '
        $0 ~ start { printf "#line %d \"%s\"\n", NR, file; found = 1 }
        found { print }
        found && /^}/ { found = 0; count++ }
        END { exit count == 1 ? 0 : 1 }
    ' "$2" >> "$3" || { echo "run_setcolor_tests: no single $1 in $2" >&2; exit 1; }
}
Q3_TEST_EXTRACT '^static byte[[:space:]]+RB_ColorByte[[:space:]]*[(]' \
    "$Q3_TEST_ROOT/code/renderer/tr_backend.c" "$Q3_TEST_DIR/setcolor_extracted.c"
Q3_TEST_EXTRACT '^const void[[:space:]]+[*]RB_SetColor[[:space:]]*[(]' \
    "$Q3_TEST_ROOT/code/renderer/tr_backend.c" "$Q3_TEST_DIR/setcolor_extracted.c"

# float-cast-overflow reports the undefined conversion the old code made.
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -I"$Q3_TEST_DIR" \
    -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all \
    "$Q3_TEST_ROOT/tests/setcolor_regression.c" -lm -o "$Q3_TEST_DIR/setcolor"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/setcolor"
