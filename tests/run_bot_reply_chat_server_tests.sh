#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-bot-reply-chat-server.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #340: a player's chat line goes through the real sv_game.c dispatcher
# into the real botlib chat AI (its own objects, built with -DBOTLIB as the
# engine links botlib). Also build with -funsigned-char, as the botlib chat
# runners do for the signed match offsets.
for Q3_TEST_MODE in host unsigned-char; do
    Q3_TEST_FLAGS=(-std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections
        -fsanitize=address,undefined)
    if [[ "$Q3_TEST_MODE" == unsigned-char ]]; then Q3_TEST_FLAGS+=(-funsigned-char); fi
    Q3_TEST_OBJECTS=()
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" -DBOTLIB \
        "-DQ3_CHAT_SOURCE=\"$Q3_TEST_ROOT/code/botlib/be_ai_chat.c\"" \
        -c "$Q3_TEST_ROOT/tests/bot_reply_chat_server_regression.c" -o "$Q3_TEST_DIR/botlib_chat.o"
    for Q3_TEST_SOURCE in l_libvar l_memory l_precomp l_script; do
        "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" -DBOTLIB -fgnu89-inline \
            -c "$Q3_TEST_ROOT/code/botlib/$Q3_TEST_SOURCE.c" -o "$Q3_TEST_DIR/$Q3_TEST_SOURCE.o"
        Q3_TEST_OBJECTS+=("$Q3_TEST_DIR/$Q3_TEST_SOURCE.o")
    done
    "${CC:-cc}" "${Q3_TEST_FLAGS[@]}" \
        "$Q3_TEST_ROOT/tests/bot_reply_chat_server_regression.c" \
        "$Q3_TEST_ROOT/code/qcommon/vm.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        "$Q3_TEST_ROOT/code/game/q_math.c" "$Q3_TEST_DIR/botlib_chat.o" "${Q3_TEST_OBJECTS[@]}" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/bot-reply-chat-server"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/bot-reply-chat-server"
done
