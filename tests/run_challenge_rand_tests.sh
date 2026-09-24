#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-challenge-rand.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #415: the server challenge, the checksum feed and the update server
# challenge shift rand() left by 16, and RAND_MAX is 0x7fffffff on glibc and
# Retro68 newlib. The fixtures include sv_client.c and cl_main.c and stub
# rand(); SV_SpawnServer needs most of the engine, so its checksum feed
# statements are taken from sv_init.c itself.
python3 - "$Q3_TEST_ROOT/code/server/sv_init.c" "$Q3_TEST_DIR/sv_checksum_feed.c" <<'PY_SOURCE'
import re, sys
from pathlib import Path
matches = re.findall(r'^\t// get a new checksum feed and restart the file system\n(.*?)^\tFS_Restart\( sv\.checksumFeed \);\n',
                     Path(sys.argv[1]).read_text(), re.M | re.S)
if len(matches) != 1 or 'sv.checksumFeed =' not in matches[0]:
    raise SystemExit("SV_SpawnServer checksum feed extraction seam no longer matches")
Path(sys.argv[2]).write_text(matches[0])
PY_SOURCE
# A signed shift that overflows must stop the run, not just print.
Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections -Wno-pointer-to-int-cast
    -fsanitize=address,undefined,shift-base,signed-integer-overflow -fno-sanitize-recover=all)
"${CC:-cc}" "${Q3_TEST_FLAGS[@]}" "-DQ3_SV_CHECKSUM_FEED=\"$Q3_TEST_DIR/sv_checksum_feed.c\"" \
    "$Q3_TEST_ROOT/tests/server_challenge_rand_regression.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/server_challenge_rand"
"${CC:-cc}" "${Q3_TEST_FLAGS[@]}" \
    "$Q3_TEST_ROOT/tests/client_challenge_rand_regression.c" \
    "$Q3_TEST_ROOT/code/qcommon/net_chan.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
    -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/client_challenge_rand"

# rand() results: RAND_MAX both times; 0x8000, the least that reaches the sign
# bit; 0x7fff, the most that does not; 0x10000, whose bits all shift out; zero.
# The last column is svs.time or Com_Milliseconds.
for Q3_TEST_CASE in "0x7fffffff 0x7fffffff 100000" "0x8000 0 0" "0x7fff 0x7fff 0x7fffffff" \
    "0x10000 0xffff 12345" "0x12345678 0x7fffffff 0x40000001" "0 0 0"; do
    for Q3_TEST_BINARY in server_challenge_rand client_challenge_rand; do
        # LeakSanitizer cannot initialize in the local ptrace sandbox.
        # shellcheck disable=SC2086 # the case is three arguments
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
        UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_BINARY" $Q3_TEST_CASE
    done
done
