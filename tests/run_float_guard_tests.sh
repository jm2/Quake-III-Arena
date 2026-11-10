#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-float-guard.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #249: the renderer's float guards, from tr_local.h verbatim (from the
# comment above R_FiniteFloat to the end of R_Ftol); the rest of tr_local.h
# needs the whole renderer.
awk -v file="$Q3_TEST_ROOT/code/renderer/tr_local.h" '
    /^\/\* Preserve valid native conversion/ { printf "#line %d \"%s\"\n", NR, file; found = 1 }
    found { print }
    found && /R_Ftol/ { inftol = 1 }
    inftol && /^}/ { found = inftol = 0; done = 1 }
    END { exit done ? 0 : 1 }
' "$Q3_TEST_ROOT/code/renderer/tr_local.h" > "$Q3_TEST_DIR/float_guard_extracted.h" ||
    { echo "run_float_guard_tests: no float guards in tr_local.h" >&2; exit 1; }
grep -Eq 'Com_Memcpy *[(]|^[[:space:]]*volatile' "$Q3_TEST_DIR/float_guard_extracted.h" &&
    { echo "run_float_guard_tests: the guards call Com_Memcpy or use volatile again (#249)" >&2; exit 1; }

"${CC:-cc}" -std=gnu99 -O2 -fno-omit-frame-pointer -I"$Q3_TEST_DIR" \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    "$Q3_TEST_ROOT/tests/float_guard_regression.c" -lm -o "$Q3_TEST_DIR/float_guard"
ASAN_OPTIONS=detect_leaks=0 "$Q3_TEST_DIR/float_guard"
