#!/usr/bin/env bash
# Issue #46: compare the shader parser with the verbatim retail 1.32c parser.
# Set Q3_SHADER_PARITY_SCRIPTS to a directory of .shader files (for example the
# scripts/ directory extracted from a retail or demo pak0.pk3 into scratch) to
# also compare every definition in them; game data is never part of the repo.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-shader-parity.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
Q3_REFERENCE="$Q3_TEST_ROOT/tests/shader_retail_parser_reference.h"
if git -C "$Q3_TEST_ROOT" cat-file -e dbe4ddb:code/renderer/tr_shader.c 2>/dev/null; then
    git -C "$Q3_TEST_ROOT" show dbe4ddb:code/renderer/tr_shader.c | sed -n '28,33p;111,1606p' > "$Q3_TEST_DIR/retail.h"
    tail -n +5 "$Q3_REFERENCE" | cmp -s - "$Q3_TEST_DIR/retail.h" || {
        echo "tests/shader_retail_parser_reference.h is not the verbatim dbe4ddb parser" >&2
        exit 1
    }
else
    echo "note: dbe4ddb is not in this checkout; using the committed retail parser reference unchecked"
fi
Q3_CORPUS=()
if [ -n "${Q3_SHADER_PARITY_SCRIPTS:-}" ]; then
    while IFS= read -r -d '' Q3_SCRIPT; do Q3_CORPUS+=("$Q3_SCRIPT"); done \
        < <(find "$Q3_SHADER_PARITY_SCRIPTS" -name '*.shader' -print0 | sort -z)
    [ "${#Q3_CORPUS[@]}" -gt 0 ] || { echo "no .shader files under $Q3_SHADER_PARITY_SCRIPTS" >&2; exit 1; }
fi
for Q3_SHADER_CONFIGURATION in sanitizer release-fast-math; do
    Q3_SHADER_FLAGS=()
    if [ "$Q3_SHADER_CONFIGURATION" = release-fast-math ]; then
        Q3_SHADER_FLAGS=(-O2 -DNDEBUG -ffast-math)
    fi
    Q3_COMPILE=("${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
        "${Q3_SHADER_FLAGS[@]}" -fsanitize=address,undefined -c)
    "${Q3_COMPILE[@]}" -DQ3_PARITY_SIDE=1 "$Q3_TEST_ROOT/tests/shader_retail_parity_regression.c" -o "$Q3_TEST_DIR/master.o"
    # The retail parser exports ParseSort and infoParms; rename them beside the real ones.
    "${Q3_COMPILE[@]}" -w -DQ3_PARITY_SIDE=2 -DParseSort=RetailParseSort -DinfoParms=retailInfoParms \
        "$Q3_TEST_ROOT/tests/shader_retail_parity_regression.c" -o "$Q3_TEST_DIR/retail.o"
    "${Q3_COMPILE[@]}" "$Q3_TEST_ROOT/tests/shader_retail_parity_regression.c" -o "$Q3_TEST_DIR/main.o"
    "${CC:-cc}" "${Q3_SHADER_FLAGS[@]}" -fsanitize=address,undefined \
        "$Q3_TEST_DIR/master.o" "$Q3_TEST_DIR/retail.o" "$Q3_TEST_DIR/main.o" \
        "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/parity"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/parity" ${Q3_CORPUS[@]+"${Q3_CORPUS[@]}"}
done
