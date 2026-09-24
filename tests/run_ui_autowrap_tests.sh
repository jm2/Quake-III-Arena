#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-ui-autowrap.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #411: the base q3_ui and Team Arena word-wrap helpers are defined next
# to the width and draw functions they call, so the fixture takes each helper
# verbatim from its real source (with a #line so reports name that file) and
# supplies those functions itself to record each line drawn.
Q3_TEST_EXTRACT() {
    awk -v start="$1" -v file="$2" '
        index($0, start) == 1 { printf "#line %d \"%s\"\n", NR, file; found = 1 }
        found { print }
        found && /^}/ { exit }
    ' "$2" > "$3"
    if ! grep -q '^}' "$3"; then
        echo "run_ui_autowrap_tests: no $1 in $2" >&2
        exit 1
    fi
}
Q3_TEST_EXTRACT 'void UI_DrawProportionalString_AutoWrapped(' \
    "$Q3_TEST_ROOT/code/q3_ui/ui_atoms.c" "$Q3_TEST_DIR/q3ui_autowrapped.c"
Q3_TEST_EXTRACT 'void Text_PaintCenter_AutoWrapped(' \
    "$Q3_TEST_ROOT/code/ui/ui_main.c" "$Q3_TEST_DIR/ui_autowrapped.c"

"${CC:-cc}" \
    -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/ui_autowrap_regression.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" "$Q3_TEST_ROOT/code/game/q_math.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_autowrap"

# One process per helper and case: ASan stops at the first read past a copy.
# "lines" is text that ends inside the copy, which must be drawn as master drew
# it; the others are copies of 1023 bytes, the most a copy holds, that end on
# each edge.
for Q3_TEST_HELPER in q3_ui ui; do
    for Q3_TEST_CASE in lines word truncated space ending narrow; do
        # LeakSanitizer cannot initialize in the local ptrace sandbox.
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_DIR/ui_autowrap" "$Q3_TEST_HELPER" "$Q3_TEST_CASE"
    done
done
