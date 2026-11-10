#!/usr/bin/env bash
# Issue #276: every CVE id cited in code/, docs/, tests/ or a top-level
# document must have a row in docs/security-provenance.md, and the ids its
# "Ids wrongly cited" section lists (ids of other products that were once
# cited here) must not appear anywhere else.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

ledger=docs/security-provenance.md
self=tests/run_cve_provenance_tests.sh
pattern='CVE-[0-9]{4}-[0-9]{4,}'
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

mapfile -d '' files < <(git ls-files -z -- code docs tests ':(glob)*.md' ':(glob)*.txt')
sources=()
for file in "${files[@]}"; do
    if [ "$file" != "$ledger" ] && [ "$file" != "$self" ] && [ -f "$file" ]; then
        sources+=("$file")
    fi
done
if [ "${#sources[@]}" -eq 0 ]; then
    echo "CVE provenance check failed: no tracked files found" >&2
    exit 1
fi

cited="$(printf '%s\0' "${sources[@]}" | xargs -0 grep -IhoE "$pattern" -- \
    | LC_ALL=C sort -u || true)"

# A cited id must be in the ledger's rows.
while IFS= read -r id; do
    [ -n "$id" ] || continue
    if ! grep -qxF -- "$id" <<<"$listed"; then
        echo "CVE provenance check failed: $id is cited but has no row in $ledger:" >&2
        printf '%s\0' "${sources[@]}" | xargs -0 grep -IlF -- "$id" | sed 's/^/    /' >&2 || true
        failed=1
    fi
done <<<"$cited"

# A wrongly cited id must not come back, in the tree or in the ledger's rows.
while IFS= read -r id; do
    if grep -qxF -- "$id" <<<"$listed"; then
        echo "CVE provenance check failed: $id is listed as wrongly cited and also has a row" >&2
        failed=1
    fi
    if grep -qxF -- "$id" <<<"$cited"; then
        echo "CVE provenance check failed: $id belongs to another product but is cited in:" >&2
        printf '%s\0' "${sources[@]}" | xargs -0 grep -IlF -- "$id" | sed 's/^/    /' >&2 || true
        failed=1
    fi
done <<<"$wrong"

if [ "$failed" -ne 0 ]; then
    exit 1
fi
echo "CVE provenance check passed: $(grep -c . <<<"$cited") cited ids, $(grep -c . <<<"$listed") ledger ids, $(grep -c . <<<"$wrong") wrongly cited ids kept out"
