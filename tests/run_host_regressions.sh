#!/usr/bin/env bash
# Run the host C regression runners (tests/run_*_tests.sh) in parallel.
#
#   tests/run_host_regressions.sh [--shard K/N] [--jobs J] [--timeout SECONDS]
#
# Runners are discovered with git ls-files, so new runners need no CI edits.
# --shard K/N selects every N-th runner (0-based K) of the sorted list. The
# compiler comes from $CC as in the individual runners. Each failing runner's
# log tail is printed and the script exits non-zero.
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export TMPDIR="${TMPDIR:-/var/tmp}"

shard=0
shards=1
jobs="$(nproc 2>/dev/null || echo 2)"
runner_timeout=900

while [ "$#" -gt 0 ]; do
    case "$1" in
        --shard)
            shard="${2%/*}"
            shards="${2#*/}"
            shift 2
            ;;
        --jobs)
            jobs="$2"
            shift 2
            ;;
        --timeout)
            runner_timeout="$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

if ! [[ "$shard" =~ ^[0-9]+$ && "$shards" =~ ^[1-9][0-9]*$ ]] || [ "$shard" -ge "$shards" ]; then
    echo "Invalid --shard value: $shard/$shards" >&2
    exit 2
fi

mapfile -t all_runners < <(git ls-files -- 'tests/run_*_tests.sh' | LC_ALL=C sort)
selected=()
for index in "${!all_runners[@]}"; do
    if [ $((index % shards)) -eq "$shard" ]; then
        selected+=("${all_runners[$index]}")
    fi
done

if [ "${#selected[@]}" -eq 0 ]; then
    echo "No runners selected for shard $shard/$shards" >&2
    exit 1
fi

log_dir="$(mktemp -d "${TMPDIR}/q3-host-regressions.XXXXXX")"
trap 'rm -rf -- "$log_dir"' EXIT

run_one() {
    local runner="$1" log_dir="$2" runner_timeout="$3"
    local name start status
    name="$(basename "$runner" .sh)"
    start="$(date +%s)"
    timeout "$runner_timeout" bash "$runner" > "$log_dir/$name.log" 2>&1
    status=$?
    printf '%s\t%s\t%s\n' "$name" "$status" "$(( $(date +%s) - start ))" > "$log_dir/$name.result"
}
export -f run_one

echo "Running ${#selected[@]} of ${#all_runners[@]} runners (shard $shard/$shards, CC=${CC:-cc}, $jobs jobs)"
printf '%s\0' "${selected[@]}" |
    xargs -0 -P "$jobs" -I{} bash -c 'run_one "$1" "$2" "$3"' _ {} "$log_dir" "$runner_timeout"

failed=0
while IFS=$'\t' read -r name status seconds; do
    if [ "$status" -eq 0 ]; then
        printf 'PASS %4ss %s\n' "$seconds" "$name"
    else
        failed=$((failed + 1))
        if [ "$status" -eq 124 ]; then
            printf 'TIME %4ss %s (exceeded %ss)\n' "$seconds" "$name" "$runner_timeout"
        else
            printf 'FAIL %4ss %s (exit %s)\n' "$seconds" "$name" "$status"
        fi
    fi
done < <(cat "$log_dir"/*.result | LC_ALL=C sort)

missing=$(( ${#selected[@]} - $(ls "$log_dir"/*.result 2>/dev/null | wc -l) ))
if [ "$missing" -ne 0 ]; then
    echo "$missing runner(s) produced no result" >&2
    failed=$((failed + missing))
fi

if [ "$failed" -ne 0 ]; then
    for result in "$log_dir"/*.result; do
        IFS=$'\t' read -r name status _ < "$result"
        if [ "$status" -ne 0 ]; then
            echo "===== $name (last 60 lines) ====="
            tail -n 60 "$log_dir/$name.log"
        fi
    done
    echo "$failed runner(s) failed" >&2
    exit 1
fi
echo "All ${#selected[@]} runners passed."
