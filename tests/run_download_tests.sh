#!/usr/bin/env bash
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-download.XXXXXX)"
Q3_TEST_CC="${CC:-cc}"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

for Q3_TEST_MODE in normal fast; do
	Q3_TEST_FLAGS=()
	if [[ "$Q3_TEST_MODE" == fast ]]; then
		Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math)
	fi

	Q3_COMMON_FLAGS=(
		-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
		-fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}"
	)
	Q3_LINK_FLAGS=(-Wl,--gc-sections -lm)

	"$Q3_TEST_CC" "${Q3_COMMON_FLAGS[@]}" \
		"-DQ3_CLIENT_MAIN_SOURCE=\"$Q3_TEST_ROOT/code/client/cl_main.c\"" \
		"$Q3_TEST_ROOT/tests/download_client_regression.c" \
		"$Q3_TEST_ROOT/code/game/q_shared.c" "${Q3_LINK_FLAGS[@]}" \
		-o "$Q3_TEST_DIR/client-$Q3_TEST_MODE"

	"$Q3_TEST_CC" "${Q3_COMMON_FLAGS[@]}" \
		"$Q3_TEST_ROOT/tests/download_server_regression.c" \
		"$Q3_TEST_ROOT/code/game/q_shared.c" "${Q3_LINK_FLAGS[@]}" \
		-o "$Q3_TEST_DIR/server-$Q3_TEST_MODE"

	"$Q3_TEST_CC" "${Q3_COMMON_FLAGS[@]}" \
		"-DQ3_CLIENT_PARSE_SOURCE=\"$Q3_TEST_ROOT/code/client/cl_parse.c\"" \
		"$Q3_TEST_ROOT/tests/download_parse_regression.c" \
		"$Q3_TEST_ROOT/code/game/q_shared.c" "${Q3_LINK_FLAGS[@]}" \
		-o "$Q3_TEST_DIR/parse-$Q3_TEST_MODE"

	"$Q3_TEST_CC" "${Q3_COMMON_FLAGS[@]}" \
		"-DQ3_FILES_SOURCE=\"$Q3_TEST_ROOT/code/qcommon/files.c\"" \
		"$Q3_TEST_ROOT/tests/download_files_regression.c" \
		"$Q3_TEST_ROOT/code/qcommon/md4.c" \
		"$Q3_TEST_ROOT/code/game/q_shared.c" "${Q3_LINK_FLAGS[@]}" -lz \
		-o "$Q3_TEST_DIR/files-$Q3_TEST_MODE"

	mkdir "$Q3_TEST_DIR/home-$Q3_TEST_MODE"
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		"$Q3_TEST_DIR/client-$Q3_TEST_MODE"
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		"$Q3_TEST_DIR/server-$Q3_TEST_MODE"
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		"$Q3_TEST_DIR/parse-$Q3_TEST_MODE"
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		"$Q3_TEST_DIR/files-$Q3_TEST_MODE" "$Q3_TEST_DIR/home-$Q3_TEST_MODE"
done
