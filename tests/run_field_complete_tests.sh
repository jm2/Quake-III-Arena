#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-field-complete.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Tab completion's FindMatches cuts shortestMatch[MAX_TOKEN_CHARS] to what
# each candidate shares with it. A server's systeminfo can create a cvar
# with a name of up to BIG_INFO_KEY bytes, so candidates longer than the
# buffer must not move the cut past it. The fixture takes common.c's
# completion statics and FindMatches verbatim (with a #line so reports name
# that file); ASan's redzones around the statics catch any stray access.
awk -v file="$Q3_TEST_ROOT/code/qcommon/common.c" '
    /^static const char \*completionString;/ { printf "#line %d \"%s\"\n", NR, file; copy = 1 }
    copy { print }
    copy && /^static void FindMatches\(/ { inFunction = 1 }
    inFunction && /^}/ { copy = 0; inFunction = 0; count++ }
    END { exit count == 1 ? 0 : 1 }
' "$Q3_TEST_ROOT/code/qcommon/common.c" > "$Q3_TEST_DIR/find_matches_extracted.c" ||
    { echo "run_field_complete_tests: no single FindMatches block in common.c" >&2; exit 1; }

"${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -fsanitize=address,undefined \
    -I"$Q3_TEST_DIR" "$Q3_TEST_ROOT/tests/field_complete_regression.c" \
    "$Q3_TEST_ROOT/code/game/q_shared.c" -lm -o "$Q3_TEST_DIR/field_complete"

ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$Q3_TEST_DIR/field_complete"
