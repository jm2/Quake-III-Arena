#!/usr/bin/env bash
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-qvm-format.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

python3 "$Q3_TEST_ROOT/tests/check_qvm_format_call_sites.py" "$Q3_TEST_ROOT"

for Q3_TEST_MODE in normal fast; do
	Q3_TEST_FLAGS=()
	if [[ "$Q3_TEST_MODE" == fast ]]; then
		Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math)
	fi
	"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer \
		-ffunction-sections -fdata-sections -fsanitize=address,undefined \
		"${Q3_TEST_FLAGS[@]}" -I"$Q3_TEST_ROOT/code/game" \
		"-DQ3_FORMATTER_SOURCE=\"$Q3_TEST_ROOT/code/game/bg_lib.c\"" \
		"$Q3_TEST_ROOT/tests/qvm_format_regression.c" \
		-Wl,--gc-sections -lm -o "$Q3_TEST_DIR/format-$Q3_TEST_MODE"
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		"$Q3_TEST_DIR/format-$Q3_TEST_MODE"
done
