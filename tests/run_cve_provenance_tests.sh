#!/usr/bin/env bash
# Issue #276: every CVE id cited in a tracked text file (code, docs, tests,
# scripts, CI) must have a row in docs/security-provenance.md, and the ids
# its "Ids wrongly cited" section lists (ids of other products that were once
# cited here) must not be cited in code/ or tests/ again. Ids are matched in
# any case; ids inside a URL are links, not citations, and are skipped.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

ledger=docs/security-provenance.md
self=tests/run_cve_provenance_tests.sh
pattern='CVE-[0-9]{4}-[0-9]{4,}'
# Print the CVE ids in the files given, upper-cased, with URLs removed first.
cve_ids() {
    sed -E 's#[a-zA-Z][a-zA-Z0-9+.-]*://[^[:space:])>"]*##g' -- "$@" 2> /dev/null \
        | grep -ioE "$pattern" | tr '[:lower:]' '[:upper:]' | LC_ALL=C sort -u || true
}
failed=0

if [ ! -f "$ledger" ]; then
    echo "CVE provenance check failed: $ledger is missing" >&2
    exit 1
fi

# Ids in the ledger outside, and inside, the wrongly-cited section.
listed="$(awk '/^## /{w = ($0 ~ /^## Ids wrongly cited/)} !w' "$ledger" \
    | grep -oE "$pattern" | LC_ALL=C sort -u || true)"
wrong="$(awk '/^## /{w = ($0 ~ /^## Ids wrongly cited/)} w' "$ledger" \
    | grep -oE "$pattern" | LC_ALL=C sort -u || true)"

if [ -z "$listed" ]; then
    echo "CVE provenance check failed: $ledger lists no CVE ids" >&2
    exit 1
fi
if [ -z "$wrong" ]; then
    echo "CVE provenance check failed: $ledger has no 'Ids wrongly cited' section" >&2
    exit 1
fi

if git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
    mapfile -d '' files < <(git ls-files -z)
else
    mapfile -d '' files < <(find . -path ./.git -prune -o -type f -print0 | sed -z 's#^\./##')
fi
sources=()
code_sources=()
for file in "${files[@]}"; do
    if [ "$file" != "$ledger" ] && [ "$file" != "$self" ] && [ -f "$file" ] &&
       grep -Iq . -- "$file" 2> /dev/null; then
        sources+=("$file")
        case "$file" in code/*|tests/*) code_sources+=("$file") ;; esac
    fi
done
if [ "${#sources[@]}" -eq 0 ]; then
    echo "CVE provenance check failed: no text files found" >&2
    exit 1
fi

cited="$(cve_ids "${sources[@]}")"
code_cited="$(cve_ids "${code_sources[@]}")"

# A cited id must be in the ledger's rows (or its wrongly-cited list, which
# the next loop keeps out of the code and tests).
while IFS= read -r id; do
    [ -n "$id" ] || continue
    if ! grep -qxF -- "$id" <<<"$listed" && ! grep -qxF -- "$id" <<<"$wrong"; then
        echo "CVE provenance check failed: $id is cited but has no row in $ledger:" >&2
        printf '%s\0' "${sources[@]}" | xargs -0 grep -IliF -- "$id" | sed 's/^/    /' >&2 || true
        failed=1
    fi
done <<<"$cited"

# A wrongly cited id must not come back in the code or the tests, or in the
# ledger's rows; docs may still mention it, e.g. in a history note.
while IFS= read -r id; do
    if grep -qxF -- "$id" <<<"$listed"; then
        echo "CVE provenance check failed: $id is listed as wrongly cited and also has a row" >&2
        failed=1
    fi
    if grep -qxF -- "$id" <<<"$code_cited"; then
        echo "CVE provenance check failed: $id belongs to another product but is cited in:" >&2
        printf '%s\0' "${code_sources[@]}" | xargs -0 grep -IliF -- "$id" | sed 's/^/    /' >&2 || true
        failed=1
    fi
done <<<"$wrong"

if [ "$failed" -ne 0 ]; then
    exit 1
fi
echo "CVE provenance check passed: $(grep -c . <<<"$cited") cited ids, $(grep -c . <<<"$listed") ledger ids, $(grep -c . <<<"$wrong") wrongly cited ids kept out"
