#!/usr/bin/env bash
# Parse every tracked shell script on its own. `bash -n a.sh b.sh` only
# parses a.sh (the rest become its positional parameters), so each file
# needs its own invocation.
#
# macOS runs build_mac.sh and the scripts it calls with /bin/bash 3.2, which
# rejects what a newer bash accepts. tests/check_bash32_compat.py looks for
# bash 4+ constructs in them; with Q3_BASH32=/path/to/bash-3.2 they are also
# parsed by that bash.
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

failed=0
checked=0
while IFS= read -r -d '' script; do
    checked=$((checked + 1))
    if ! bash -n "$script"; then
        echo "Syntax error: $script" >&2
        failed=1
    fi
done < <(git ls-files -z -- '*.sh')

if [ "$checked" -eq 0 ]; then
    echo "No tracked shell scripts found" >&2
    exit 1
fi
echo "Parsed $checked shell scripts."

if ! python3 tests/check_bash32_compat.py --self-test; then
    echo "bash 4+ constructs in scripts macOS runs with bash 3.2 (see above)" >&2
    failed=1
fi

if [ -n "${Q3_BASH32:-}" ]; then
    if ! "$Q3_BASH32" -c 'case "$BASH_VERSION" in (3.2.*) exit 0 ;; esac; exit 1'; then
        echo "Q3_BASH32=$Q3_BASH32 is not a bash 3.2" >&2
        exit 1
    fi
    checked=0
    while IFS= read -r script; do
        checked=$((checked + 1))
        if ! "$Q3_BASH32" -n "$script"; then
            echo "bash 3.2 syntax error: $script" >&2
            failed=1
        fi
    done < <(python3 tests/check_bash32_compat.py --list)
    echo "Parsed $checked macOS-facing scripts with $Q3_BASH32."
fi
exit "$failed"
