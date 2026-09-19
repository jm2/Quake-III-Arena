#!/usr/bin/env bash
set -euo pipefail

export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-format-string.XXXXXX)"
Q3_FORMAT_COMMON_INPUT="${Q3_FORMAT_COMMON_INPUT:-$Q3_TEST_ROOT/code/qcommon/common.c}"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

python3 "$Q3_TEST_ROOT/tests/check_format_call_sites.py" "$Q3_TEST_ROOT"

# Isolate the production Com_Error body while supplying its output callbacks
# from the harness. All other production sections are discarded at link time.
python3 - "$Q3_FORMAT_COMMON_INPUT" "$Q3_TEST_DIR/common.c" <<'PY_SOURCE'
import re
import sys
from pathlib import Path

source = Path(sys.argv[1]).read_text(encoding="utf-8")
for name in ("Com_Printf", "Com_Shutdown"):
    source, count = re.subn(
        r"(?m)^(void(?: QDECL)? )" + name + r"(\s*\()",
        r"\1FixtureUnused_" + name + r"\2",
        source,
    )
    assert count == 1, (name, count)
Path(sys.argv[2]).write_text(source, encoding="utf-8")
PY_SOURCE

Q3_SANITIZER_ENV=(
	env
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
)

for Q3_TEST_MODE in normal fast; do
	Q3_TEST_FLAGS=()
	if [[ "$Q3_TEST_MODE" == fast ]]; then
		Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math)
	fi
	Q3_COMMON_FLAGS=(
		-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
		-fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}"
		-I"$Q3_TEST_ROOT/code/qcommon" -I"$Q3_TEST_ROOT/code/game"
	)

	"${CC:-cc}" "${Q3_COMMON_FLAGS[@]}" \
		"-DQ3_FORMAT_COMMON_SOURCE=\"$Q3_TEST_DIR/common.c\"" \
		"$Q3_TEST_ROOT/tests/format_error_regression.c" \
		-Wl,--gc-sections -lm -o "$Q3_TEST_DIR/error-$Q3_TEST_MODE"
	"${Q3_SANITIZER_ENV[@]}" "$Q3_TEST_DIR/error-$Q3_TEST_MODE"

	"${CC:-cc}" -DBOTLIB "${Q3_COMMON_FLAGS[@]}" \
		-I"$Q3_TEST_ROOT/code/botlib" \
		"-DQ3_FORMAT_AAS_SOURCE=\"$Q3_TEST_ROOT/code/botlib/be_aas_main.c\"" \
		"$Q3_TEST_ROOT/tests/format_aas_regression.c" \
		-Wl,--gc-sections -lm -o "$Q3_TEST_DIR/aas-$Q3_TEST_MODE"
	"${Q3_SANITIZER_ENV[@]}" "$Q3_TEST_DIR/aas-$Q3_TEST_MODE"

	"${CC:-cc}" -DBOTLIB "${Q3_COMMON_FLAGS[@]}" \
		-I"$Q3_TEST_ROOT/code/botlib" \
		"-DQ3_FORMAT_SCRIPT_SOURCE=\"$Q3_TEST_ROOT/code/botlib/l_script.c\"" \
		"$Q3_TEST_ROOT/tests/format_script_regression.c" \
		"$Q3_TEST_ROOT/code/game/q_shared.c" \
		-Wl,--gc-sections -lm -o "$Q3_TEST_DIR/script-$Q3_TEST_MODE"
	"${Q3_SANITIZER_ENV[@]}" "$Q3_TEST_DIR/script-$Q3_TEST_MODE"

	"${CC:-cc}" "${Q3_COMMON_FLAGS[@]}" \
		"-DQ3_FORMAT_NET_SOURCE=\"$Q3_TEST_ROOT/code/qcommon/net_chan.c\"" \
		"$Q3_TEST_ROOT/tests/format_net_regression.c" \
		-Wl,--gc-sections -lm -o "$Q3_TEST_DIR/net-$Q3_TEST_MODE"
	"${Q3_SANITIZER_ENV[@]}" "$Q3_TEST_DIR/net-$Q3_TEST_MODE"
done
