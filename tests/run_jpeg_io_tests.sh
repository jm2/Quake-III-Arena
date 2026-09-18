#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-jpeg-io.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
Q3_JPEG_SOURCES=()
for Q3_JPEG_PATH in "$Q3_TEST_ROOT"/code/jpeg-6/*.c; do
    case "${Q3_JPEG_PATH##*/}" in
        jmemansi.c|jmemdos.c|jmemname.c|jpegtran.c) continue ;;
    esac
    Q3_JPEG_SOURCES+=("$Q3_JPEG_PATH")
done
"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined "$Q3_TEST_ROOT/tests/jpeg_io_regression.c" \
    "${Q3_JPEG_SOURCES[@]}" -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/jpeg"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/jpeg" "$Q3_TEST_DIR/stdio.jpg"
