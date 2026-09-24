#!/usr/bin/env bash
set -euo pipefail

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d "${TMPDIR:-/var/tmp}/q3-protected-cvar.XXXXXX")"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Issue #39: modules and servers must not move the filesystem paths or remove
# engine commands. The fixtures drive the real game, cgame and UI dispatchers
# (as a QVM and through VM_DllSyscall as a native module) and the real
# CL_SystemInfoChanged against the real cvar.c and cmd.c. The filesystem cvars
# come from FS_Startup itself, so the fixtures see the flags files.c gives
# them, and the command line goes through the real Com_StartupVariable.
python3 - "$Q3_TEST_ROOT/code/qcommon" "$Q3_TEST_DIR" <<'PY_SOURCE'
import re, sys
from pathlib import Path
seams = (
    ("files.c", r'^\tfs_debug = Cvar_Get\(.*?^\tfs_restrict = Cvar_Get [^\n]*\n', "fs_startup_cvars.c"),
    ("common.c", r'^void Com_StartupVariable\( const char \*match \) \{\n.*?^\}\n', "com_startup_variable.c"),
)
for source, pattern, output in seams:
    matches = re.findall(pattern, Path(sys.argv[1], source).read_text(encoding="latin-1"), re.M | re.S)
    if len(matches) != 1:
        raise SystemExit(source + " extraction seam no longer matches")
    Path(sys.argv[2], output).write_text(matches[0], encoding="latin-1")
PY_SOURCE

for Q3_TEST_FIXTURE in client game; do
    Q3_TEST_EXTRA=("$Q3_TEST_ROOT/code/game/q_math.c")
    if [[ "$Q3_TEST_FIXTURE" == client ]]; then
        Q3_TEST_EXTRA=("$Q3_TEST_ROOT/code/client/cl_ui.c" "$Q3_TEST_ROOT/code/client/cl_parse.c")
    fi
    "${CC:-cc}" \
        -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined \
        "-DQ3_FS_STARTUP_CVARS=\"$Q3_TEST_DIR/fs_startup_cvars.c\"" \
        "-DQ3_COM_STARTUP_VARIABLE=\"$Q3_TEST_DIR/com_startup_variable.c\"" \
        "$Q3_TEST_ROOT/tests/protected_cvar_${Q3_TEST_FIXTURE}_regression.c" "${Q3_TEST_EXTRA[@]}" \
        "$Q3_TEST_ROOT/code/qcommon/cvar.c" "$Q3_TEST_ROOT/code/qcommon/cmd.c" \
        "$Q3_TEST_ROOT/code/qcommon/vm.c" "$Q3_TEST_ROOT/code/game/q_shared.c" \
        -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/$Q3_TEST_FIXTURE"

    # LeakSanitizer cannot initialize in the local ptrace sandbox.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_DIR/$Q3_TEST_FIXTURE"
done
