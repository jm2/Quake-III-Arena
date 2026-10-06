#!/usr/bin/env bash
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-mac-event-queue.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issues #256, #291 and #18: classic Mac modifier keys, suspend/resume and the
# event queue.  mac_main.c, mac_event.c and mac_input.c need the whole Mac
# Toolbox, so the fixture takes the event queue (from "// Event Queue" to the
# "// mac_event.c" line after it) and Sys_GetEvent from mac_main.c, the
# functions that turn Event Manager events into key events from mac_event.c,
# and InputSprocket suspend/resume and polling from mac_input.c, verbatim
# (with a #line so reports name those files), and fakes the Toolbox and
# InputSprocket around them.  Sys_QueEvent is renamed so the fixture can
# wrap it and fail any Com_Printf from inside the queue.
Q3_TEST_EXTRACT() {
    awk -v start="$1" -v file="$2" '
        $0 ~ start { printf "#line %d \"%s\"\n", NR, file; found = 1 }
        found { print }
        found && /^}/ { found = 0; count++ }
        END { exit count == 1 ? 0 : 1 }
    ' "$2" >> "$3" || { echo "run_mac_event_queue_tests: no single $1 in $2" >&2; exit 1; }
}
Q3_TEST_MAIN="$Q3_TEST_ROOT/code/mac/mac_main.c"
Q3_TEST_EVENT="$Q3_TEST_ROOT/code/mac/mac_event.c"
awk -v file="$Q3_TEST_MAIN" '
    /^\/\/ Event Queue/ { printf "#line %d \"%s\"\n", NR, file; found = 1; count++ }
    found && /^\/\/ mac_event[.]c/ { found = 0 }
    found { print }
    END { exit count == 1 ? 0 : 1 }
' "$Q3_TEST_MAIN" > "$Q3_TEST_DIR/mac_main_extracted.c" ||
    { echo "run_mac_event_queue_tests: no single event queue in $Q3_TEST_MAIN" >&2; exit 1; }
Q3_TEST_EXTRACT '^sysEvent_t[[:space:]]+Sys_GetEvent[[:space:]]*[(]' "$Q3_TEST_MAIN" "$Q3_TEST_DIR/mac_main_extracted.c"
sed -i 's/^void Sys_QueEvent( int time,/void Sys_QueEvent_extracted( int time,/' "$Q3_TEST_DIR/mac_main_extracted.c"
[ "$(grep -c '^void Sys_QueEvent_extracted(' "$Q3_TEST_DIR/mac_main_extracted.c")" = 1 ] ||
    { echo "run_mac_event_queue_tests: no single Sys_QueEvent definition in $Q3_TEST_MAIN" >&2; exit 1; }
: > "$Q3_TEST_DIR/mac_event_extracted.c"
Q3_TEST_EXTRACT '^int[[:space:]]+Sys_MsecForMacEvent[[:space:]]*[(][^;]*$' "$Q3_TEST_EVENT" "$Q3_TEST_DIR/mac_event_extracted.c"
Q3_TEST_EXTRACT '^int[[:space:]]+vkeyToQuakeKey[[:space:]]*[[]' "$Q3_TEST_EVENT" "$Q3_TEST_DIR/mac_event_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+DoKeyDown[[:space:]]*[(][^;]*$' "$Q3_TEST_EVENT" "$Q3_TEST_DIR/mac_event_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+DoKeyUp[[:space:]]*[(][^;]*$' "$Q3_TEST_EVENT" "$Q3_TEST_DIR/mac_event_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+Sys_ModifierEvents[[:space:]]*[(][^;]*$' "$Q3_TEST_EVENT" "$Q3_TEST_DIR/mac_event_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+DoOSEvent[[:space:]]*[(][^;]*$' "$Q3_TEST_EVENT" "$Q3_TEST_DIR/mac_event_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+Sys_SendKeyEvents[[:space:]]*[(][^;]*$' "$Q3_TEST_EVENT" "$Q3_TEST_DIR/mac_event_extracted.c"
Q3_TEST_INPUT="$Q3_TEST_ROOT/code/mac/mac_input.c"
: > "$Q3_TEST_DIR/mac_input_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+Sys_SuspendInput[[:space:]]*[(][^;]*$' "$Q3_TEST_INPUT" "$Q3_TEST_DIR/mac_input_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+Sys_ResumeInput[[:space:]]*[(][^;]*$' "$Q3_TEST_INPUT" "$Q3_TEST_DIR/mac_input_extracted.c"
Q3_TEST_EXTRACT '^void[[:space:]]+Sys_Input[[:space:]]*[(][^;]*$' "$Q3_TEST_INPUT" "$Q3_TEST_DIR/mac_input_extracted.c"

"${CC:-cc}" \
    -std=gnu99 -fsigned-char -Wall -Wno-unused-function -Wno-unused-but-set-variable -fno-omit-frame-pointer \
    -fsanitize=address,undefined -I"$Q3_TEST_DIR" \
    "$Q3_TEST_ROOT/tests/mac_event_queue_regression.c" \
    -o "$Q3_TEST_DIR/mac_event_queue"

Q3_TEST_STATUS=0
for Q3_TEST_CASE in modifier-alone modifier-alone-fullscreen background-modifiers \
        suspend-releases isp-press-at-suspend isp-press-during-suspend no-isp \
        overflow overflow-releases; do
    ASAN_OPTIONS=detect_leaks=${Q3_TEST_DETECT_LEAKS:-1}:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        "$Q3_TEST_DIR/mac_event_queue" "$Q3_TEST_CASE" || Q3_TEST_STATUS=1
done
exit "$Q3_TEST_STATUS"
