#!/usr/bin/env bash
# Issue #235: a Finder launch of Quake3_TeamArena has no command line, so the
# executable itself must mount missionpack over baseq3, as retail's
# "+set fs_game missionpack" did; Quake3 must still mount baseq3 alone, and a
# command line fs_game must still win in both.  The Team Arena build takes
# DEFAULT_FS_GAME from the Quake3_TeamArena target in CMakeLists.txt.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-fs-default-game.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

# Com_StartupVariable from common.c itself, the Quake3_TeamArena definition
# from CMakeLists.txt (and none for Quake3), and an install with baseq3,
# missionpack and a mod.
python3 - "$Q3_TEST_ROOT" "$Q3_TEST_DIR" <<'PY_SETUP'
import re, sys, zipfile
from pathlib import Path
root, out = Path(sys.argv[1]), Path(sys.argv[2])
common = (root / "code/qcommon/common.c").read_text(encoding="latin-1")
found = re.findall(r'^void Com_StartupVariable\( const char \*match \) \{\n.*?^\}\n', common, re.M | re.S)
if len(found) != 1:
    raise SystemExit("common.c extraction seam no longer matches")
(out / "com_startup_variable.c").write_text(found[0], encoding="latin-1")
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
uses = re.findall(r'target_compile_definitions\((\w+)[^)]*?DEFAULT_FS_GAME="([^"]*)"', cmake, re.S)
if uses != [("Quake3_TeamArena", "missionpack")] or cmake.count("DEFAULT_FS_GAME=") != 1:
    raise SystemExit("CMakeLists.txt must give only Quake3_TeamArena DEFAULT_FS_GAME, found %r" % uses)
(out / "ta_default").write_text(uses[0][1])
text = (b"This file is copyright 1999 Id Software, and may not be duplicated except "
        b"during a licensed installation of the full commercial version of Quake 3:Arena")
def pack(game, entries):
    (out / "install" / game).mkdir(parents=True)
    with zipfile.ZipFile(out / "install" / game / "pak0.pk3", "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in entries:
            archive.writestr(name, data)
pack("baseq3", [("default.cfg", b"// default\n"), ("productid.txt", text)])
pack("missionpack", [("ui/menus.txt", b"{ loadmenu { \"ui/main.menu\" } }\n")])
pack("mymod", [("mymod.txt", b"mod\n")])
PY_SETUP

Q3_TA_DEFAULT="$(cat "$Q3_TEST_DIR/ta_default")"
for Q3_TEST_BUILD in quake3 teamarena; do
    for Q3_TEST_MODE in normal fast; do
        Q3_TEST_FLAGS=()
        if [[ "$Q3_TEST_BUILD" == teamarena ]]; then Q3_TEST_FLAGS+=("-DDEFAULT_FS_GAME=\"$Q3_TA_DEFAULT\""); fi
        if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS+=(-O2 -DNDEBUG -ffast-math); fi
        Q3_TEST_BIN="$Q3_TEST_DIR/default-game-$Q3_TEST_BUILD-$Q3_TEST_MODE"
        "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
            -fsanitize=address,undefined "${Q3_TEST_FLAGS[@]}" \
            "-DQ3_COM_STARTUP_VARIABLE=\"$Q3_TEST_DIR/com_startup_variable.c\"" \
            "$Q3_TEST_ROOT/tests/fs_default_game_regression.c" \
            "$Q3_TEST_ROOT/code/qcommon/cvar.c" "$Q3_TEST_ROOT/code/qcommon/cmd.c" \
            "$Q3_TEST_ROOT/code/qcommon/unzip.c" "$Q3_TEST_ROOT/code/qcommon/md4.c" \
            "$Q3_TEST_ROOT/code/game/q_shared.c" \
            -Wl,--gc-sections -lm -o "$Q3_TEST_BIN"
        Q3_DEFAULT_GAME=baseq3
        if [[ "$Q3_TEST_BUILD" == teamarena ]]; then Q3_DEFAULT_GAME=missionpack; fi
        Q3_RUN=(env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
                UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$Q3_TEST_BIN" "$Q3_TEST_DIR/install")
        # A Finder launch: no command line at all.
        "${Q3_RUN[@]}" "$Q3_DEFAULT_GAME"
        # Other command line sets leave the default alone.
        "${Q3_RUN[@]}" "$Q3_DEFAULT_GAME" "set r_mode 4" "set com_hunkmegs 64"
        # A command line fs_game wins, even an empty one.
        "${Q3_RUN[@]}" mymod "set fs_game mymod"
        "${Q3_RUN[@]}" baseq3 'set fs_game ""'
        "${Q3_RUN[@]}" missionpack "set fs_game missionpack"
        # The last of several sets wins, as with any command line cvar.
        "${Q3_RUN[@]}" mymod "set fs_game missionpack" "set fs_game mymod"
    done
done
