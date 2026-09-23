#!/usr/bin/env bash
# Parse every tracked shell script on its own. `bash -n a.sh b.sh` only
# parses a.sh (the rest become its positional parameters), so each file
# needs its own invocation.
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
exit "$failed"
