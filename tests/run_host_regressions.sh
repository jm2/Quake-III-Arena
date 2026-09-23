#!/usr/bin/env bash
# Run the host C regression runners (tests/run_*_tests.sh) in parallel.
#
#   tests/run_host_regressions.sh [--shard K/N] [--jobs J] [--timeout SECONDS]
#                                 [--deadline SECONDS]
#
# Runners are discovered with git ls-files, so new runners need no CI edits.
# --shard K/N selects every N-th runner (0-based K) of the sorted list. The
# compiler comes from $CC as in the individual runners. Each result is printed
# as soon as its runner finishes. --timeout bounds one runner; --deadline
# bounds the whole run, after which every unfinished runner is named. Each
# failing runner's full log is printed and the script exits non-zero.
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
export TMPDIR="${TMPDIR:-/var/tmp}"

shard=0
shards=1
jobs="$(nproc 2>/dev/null || echo 2)"
runner_timeout=600
deadline=0

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
        --deadline)
            deadline="$2"
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
    local name start status seconds label
    name="$(basename "$runner" .sh)"
    start="$(date +%s)"
    timeout -k 30 "$runner_timeout" bash "$runner" > "$log_dir/$name.log" 2>&1
    status=$?
    seconds=$(( $(date +%s) - start ))
    printf '%s\t%s\t%s\n' "$name" "$status" "$seconds" > "$log_dir/$name.result"
    case "$status" in
        0) label=PASS ;;
        124|137) label=TIME ;;
        *) label=FAIL ;;
    esac
    printf '%s %4ss %s (exit %s)\n' "$label" "$seconds" "$name" "$status"
}
export -f run_one

deadline_cmd=()
deadline_note=""
if [ "$deadline" -gt 0 ]; then
    # Runners that are still going at the deadline are reported as NORESULT.
    # Their own timeouts may outlive this script in a local run; CI tears the
    # job down.
    deadline_cmd=(timeout -k 15 "$deadline")
    deadline_note=", ${deadline}s deadline"
fi
echo "Running ${#selected[@]} of ${#all_runners[@]} runners (shard $shard/$shards, CC=${CC:-cc}, $jobs jobs, ${runner_timeout}s per runner$deadline_note)"
printf '%s\0' "${selected[@]}" |
    "${deadline_cmd[@]}" xargs -0 -P "$jobs" -I{} bash -c 'run_one "$1" "$2" "$3"' _ {} "$log_dir" "$runner_timeout"

failed=0
failed_names=()
for runner in "${selected[@]}"; do
    name="$(basename "$runner" .sh)"
    if [ ! -f "$log_dir/$name.result" ]; then
        echo "NORESULT $name (did not finish before the deadline or was killed)"
        failed=$((failed + 1))
        failed_names+=("$name")
        continue
    fi
    IFS=$'\t' read -r _ status _ < "$log_dir/$name.result"
    if [ "$status" -ne 0 ]; then
        failed=$((failed + 1))
        failed_names+=("$name")
    fi
done

if [ "$failed" -ne 0 ]; then
    for name in "${failed_names[@]}"; do
        echo "===== $name (full log) ====="
        cat "$log_dir/$name.log" 2>/dev/null || echo "(no log)"
    done
    echo "$failed runner(s) failed: ${failed_names[*]}" >&2
    exit 1
fi
echo "All ${#selected[@]} runners passed."
