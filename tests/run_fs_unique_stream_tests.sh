#!/usr/bin/env bash
# Unique pk3 reads stream through their own archive within a small zone (#260).
# Set Q3_UNIQUE_STREAM_PK3 to a real pk3 (for example the demo pak0.pk3) to
# also stream every entry of it; game data is never part of the repository.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-unique-stream.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
python3 "$Q3_TEST_ROOT/tests/create_fs_unique_stream_fixtures.py" "$Q3_TEST_DIR"
if [[ -n "${Q3_UNIQUE_STREAM_PK3:-}" ]]; then
    python3 "$Q3_TEST_ROOT/tests/create_fs_unique_stream_fixtures.py" --manifest \
        "$Q3_UNIQUE_STREAM_PK3" "$Q3_TEST_DIR/real.manifest"
fi
for Q3_TEST_MODE in normal fast; do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math); fi
    "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/fs_unique_stream_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/md4.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/unique-stream-tests"
    run() {
        ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
            UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_DIR/unique-stream-tests" "$@"
    }
    run stream "$Q3_TEST_DIR/crafted.pk3" "$Q3_TEST_DIR/crafted.manifest"
    run concurrent "$Q3_TEST_DIR/crafted.pk3" "$Q3_TEST_DIR/crafted.manifest"
    run corrupt "$Q3_TEST_DIR/corrupt.pk3" "$Q3_TEST_DIR/corrupt.manifest" "$Q3_TEST_DIR/corrupt.prefix"
    run truncated "$Q3_TEST_DIR/crafted.pk3" "$Q3_TEST_DIR/crafted.manifest" "$Q3_TEST_DIR/truncated.pk3"
    run fallback "$Q3_TEST_DIR/crafted.pk3" "$Q3_TEST_DIR/crafted.manifest" "$Q3_TEST_DIR/moved.pk3"
    run restart "$Q3_TEST_DIR/crafted.pk3" "$Q3_TEST_DIR/crafted.manifest" "$Q3_TEST_DIR/moved.pk3"
    if [[ -n "${Q3_UNIQUE_STREAM_PK3:-}" ]]; then
        run stream "$Q3_UNIQUE_STREAM_PK3" "$Q3_TEST_DIR/real.manifest"
        run concurrent "$Q3_UNIQUE_STREAM_PK3" "$Q3_TEST_DIR/real.manifest"
    fi
done
