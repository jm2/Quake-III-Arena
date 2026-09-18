#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-bot-game-map.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
python3 - "$Q3_TEST_ROOT" "$Q3_TEST_DIR/bodies.h" <<'PY'
from pathlib import Path
import sys,re
root=Path(sys.argv[1]);ai=(root/'code/game/ai_main.c').read_text();game=(root/'code/game/g_main.c').read_text()
def body(source,signature):
    start=re.search(re.escape(signature)+r'[^\n;]*\{',source).start();end=source.index('\n}\n',start)+3
    return source[start:end]
parts=[body(ai,'int BotAILoadMap('),body(ai,'int BotAISetup('),body(ai,'int BotAIShutdown('),body(game,'void G_InitGame(')]
# Keep each original entry prefix exactly up to its first side-effect/import.
# A sentinel ends the successful prefix; the remaining body is cross-built.
for signature,marker,sentinel in [('int BotAIStartFrame(','\tG_CheckBotSpawn();',73),('int BotAISetupClient(','\tif (!botstates[client])',74)]:
    original=body(ai,signature);parts.append(original[:original.index(marker)]+f'\treturn {sentinel};\n}}\n')
Path(sys.argv[2]).write_text('\n'.join(parts))
PY
for Q3_TEST_MODE in normal fast missionpack missionpack-fast;do
    Q3_TEST_FLAGS=()
    if [[ "$Q3_TEST_MODE" == *fast ]];then Q3_TEST_FLAGS=(-O2 -DNDEBUG -ffast-math);fi
    if [[ "$Q3_TEST_MODE" == missionpack* ]];then Q3_TEST_FLAGS+=(-DMISSIONPACK);fi
    "${CC:-cc}" -std=gnu99 -fgnu89-inline -fno-omit-frame-pointer \
        -fsanitize=address,undefined,float-cast-overflow "${Q3_TEST_FLAGS[@]}" \
        "-DQ3_GAME_MAP_BODIES=\"$Q3_TEST_DIR/bodies.h\"" \
        "$Q3_TEST_ROOT/tests/bot_game_map_regression.c" -lm -o "$Q3_TEST_DIR/test"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/test"
done
