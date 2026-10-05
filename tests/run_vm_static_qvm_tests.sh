#!/usr/bin/env bash
# Issue #13: the static build's linked-in game modules stand in only for the
# stock QVMs of their own game; a mod's QVMs (fs_game, a pure server's pk3, a
# loose file) run in the interpreter, as retail ran the search path's QVM.
# Builds tests/vm_static_qvm_regression.c around the real files.c, unzip.c,
# vm.c, vm_interpreted.c, vm_static.c, cvar.c and cmd.c, as Quake3 and as
# Quake3_TeamArena (the STATIC_MODULES_GAME CMakeLists.txt gives it), over a
# generated install whose baseq3/pak0.pk3 and missionpack/pak3.pk3 carry the
# retail header checksums: each holds the fixture QVMs plus two entries whose
# CRC-32s are chosen so that the MD4 of the pk3's CRC list (files.c's header
# checksum) is the retail value.
# Issue #325: tests/cross_game_qvm_regression.c then drives the real connect
# path (CL_ParseGamestate's systeminfo, FS_ConditionalRestart, VM_Create) over
# the same install for each executable, joining the other game's server, its
# own game's and a baseq3 mod's, and disconnecting.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"
Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-vm-static-qvm.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT

python3 - "$Q3_TEST_ROOT" "$Q3_TEST_DIR" <<'PY_SETUP'
import re, struct, sys, zipfile, zlib
from pathlib import Path
root, out = Path(sys.argv[1]), Path(sys.argv[2])

# Quake3_TeamArena's modules replace missionpack's QVMs; Quake3 has no
# definition and so replaces baseq3's (qcommon.h's default).
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
uses = re.findall(r'target_compile_definitions\((\w+)[^)]*?STATIC_MODULES_GAME="([^"]*)"', cmake, re.S)
if uses != [("Quake3_TeamArena", "missionpack")] or cmake.count("STATIC_MODULES_GAME=") != 1:
    raise SystemExit("CMakeLists.txt must give only Quake3_TeamArena STATIC_MODULES_GAME, found %r" % uses)
(out / "ta_game").write_text(uses[0][1])
# Issue #325: Quake3_TeamArena also starts on missionpack (#487).
uses = re.findall(r'target_compile_definitions\((\w+)[^)]*?DEFAULT_FS_GAME="([^"]*)"', cmake, re.S)
if uses != [("Quake3_TeamArena", "missionpack")] or cmake.count("DEFAULT_FS_GAME=") != 1:
    raise SystemExit("CMakeLists.txt must give only Quake3_TeamArena DEFAULT_FS_GAME, found %r" % uses)
(out / "ta_default").write_text(uses[0][1])
# cross_game_qvm_regression.c models a disconnect as keeping fs_game, as
# retail's CL_Disconnect did: the engine sets fs_game only from a server's
# systeminfo (CL_SystemInfoChanged) and when FS_Startup refuses a bad one.
sets = []
for path in sorted((root / "code").rglob("*.c")):
    if path.relative_to(root / "code").parts[0] in ("game", "cgame", "ui", "q3_ui"):
        continue
    for line in path.read_text(encoding="latin-1").splitlines():
        if re.search(r'Cvar_\w+\s*\(\s*"fs_game"', line) and not re.search(r'Cvar_(Get|VariableString)\b', line):
            sets.append("%s: %s" % (path.relative_to(root), line.strip()))
if sets != ['code/client/cl_parse.c: Cvar_Set( "fs_game", "" );', 'code/qcommon/files.c: Cvar_Set( "fs_game", "" );']:
    raise SystemExit("engine code that sets fs_game changed: %r" % sets)

# The call sites pass retail's interpret modes and let VM_Create choose; a
# static build used to force VMI_NATIVE there.
for path, cvar in (("code/client/cl_cgame.c", "vm_cgame"), ("code/client/cl_ui.c", "vm_ui"),
                   ("code/server/sv_game.c", "vm_game")):
    text = (root / path).read_text(encoding="latin-1")
    if re.search(r'#ifdef\s+(CGAME|UI|GAME)_HARD_LINKED', text) or \
       ('Cvar_VariableValue( "%s" )' % cvar) not in text:
        raise SystemExit("%s must pass %s to VM_Create in the static build too" % (path, cvar))

# vm_local.h's opcode numbers for the fixture QVM.
local = (root / "code/qcommon/vm_local.h").read_text(encoding="latin-1")
body = re.search(r'typedef enum \{(.*?)\} opcode_t;', local, re.S).group(1)
ops = re.findall(r'\b(OP_\w+)', re.sub(r'//[^\n]*', '', body))
OP = {name: i for i, name in enumerate(ops)}

def qvm(value):
    """vmMain returns value: ENTER 8; CONST value; LEAVE 8."""
    code = bytes([OP["OP_ENTER"]]) + struct.pack("<i", 8) + bytes([OP["OP_CONST"]]) + \
        struct.pack("<i", value) + bytes([OP["OP_LEAVE"]]) + struct.pack("<i", 8) + b"\0"
    data = struct.pack("<i", 0x5a5a5a5a)
    header = struct.pack("<8i", 0x12721444, 3, 32, len(code), 32 + len(code), len(data), 0, 65536)
    return header + code + data

def forge(crc):
    """Four bytes whose CRC-32 is crc (CRC-32 is affine in a fixed-length input)."""
    base = zlib.crc32(b"\0\0\0\0")
    rows = []
    for bit in range(32):
        rows.append((zlib.crc32(((1 << bit)).to_bytes(4, "little")) ^ base, 1 << bit))
    want, result = crc ^ base, 0
    for i in range(32):
        pivot = next(j for j in range(i, 32) if rows[j][0] >> i & 1)
        rows[i], rows[pivot] = rows[pivot], rows[i]
        for j in range(32):
            if j != i and rows[j][0] >> i & 1:
                rows[j] = (rows[j][0] ^ rows[i][0], rows[j][1] ^ rows[i][1])
    for i in range(32):
        if want >> i & 1:
            result ^= rows[i][1]
    data = result.to_bytes(4, "little")
    assert zlib.crc32(data) == crc
    return data

def pack(path, entries):
    path = out / "install" / path
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in entries:
            archive.writestr(name, data)

def stock(base):
    return [("vm/cgame.qvm", qvm(base + 1)), ("vm/qagame.qvm", qvm(base + 2)), ("vm/ui.qvm", qvm(base + 3))]

productid = (b"This file is copyright 1999 Id Software, and may not be duplicated except "
             b"during a licensed installation of the full commercial version of Quake 3:Arena")
# One pk3 per game directory: FS_AddGameDirectory sorts pk3 names with the
# ILP32 element size.  The full game's files are loose in baseq3.
(out / "install" / "baseq3").mkdir(parents=True)
(out / "install" / "baseq3" / "default.cfg").write_bytes(b"// default\n")
(out / "install" / "baseq3" / "productid.txt").write_bytes(productid)
# The last two entries' CRCs (found by searching the last one for a few
# values of the other) complete retail baseq3/pak0.pk3's header checksum
# 1566731103 and missionpack/pak3.pk3's 1438664554 (FS_IdPakGame) for these
# exact fixture QVMs; a changed fixture QVM needs new values (the test says
# so and prints the checksum it got).
pack("baseq3/pak0.pk3", stock(100) + [("salt.dat", forge(1)), ("forge.dat", forge(0x78aa8e5f))])
pack("missionpack/pak3.pk3", stock(300) + [("salt.dat", forge(4)), ("forge.dat", forge(0xac6d28fb))])
pack("mymod/zmod.pk3", [("vm/cgame.qvm", qvm(201))])
(out / "install" / "loosemod" / "vm").mkdir(parents=True)
(out / "install" / "loosemod" / "vm" / "cgame.qvm").write_bytes(qvm(401))
PY_SETUP

Q3_TA_GAME="$(cat "$Q3_TEST_DIR/ta_game")"
Q3_TA_DEFAULT="$(cat "$Q3_TEST_DIR/ta_default")"
# The md4.c the static-QVM build links (an ILP32 copy while md4.c's UINT4 is
# 64 bits on LP64 hosts; md4.c itself once PR #496 fixes that).
Q3_TEST_MD4="$Q3_TEST_ROOT/code/qcommon/md4.c"
if [[ -f "$Q3_TEST_DIR/md4.c" ]]; then Q3_TEST_MD4="$Q3_TEST_DIR/md4.c"; fi
for Q3_TEST_BUILD in quake3 teamarena; do
    for Q3_TEST_MODE in normal fast; do
        Q3_TEST_FLAGS=(-DQ3_STATIC -I"$Q3_TEST_ROOT/code/qcommon")
        if [[ "$Q3_TEST_BUILD" == teamarena ]]; then Q3_TEST_FLAGS+=("-DSTATIC_MODULES_GAME=\"$Q3_TA_GAME\""); fi
        if [[ "$Q3_TEST_MODE" == fast ]]; then Q3_TEST_FLAGS+=(-O2 -DNDEBUG); fi
        Q3_TEST_BIN="$Q3_TEST_DIR/static-qvm-$Q3_TEST_BUILD-$Q3_TEST_MODE"
        "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
            -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
            -fsanitize=address,undefined -fno-sanitize=alignment "${Q3_TEST_FLAGS[@]}" \
            "$Q3_TEST_ROOT/tests/vm_static_qvm_regression.c" \
            "$Q3_TEST_ROOT/code/qcommon/vm.c" "$Q3_TEST_ROOT/code/qcommon/vm_interpreted.c" \
            "$Q3_TEST_ROOT/code/qcommon/vm_static.c" \
            "$Q3_TEST_ROOT/code/qcommon/cvar.c" "$Q3_TEST_ROOT/code/qcommon/cmd.c" \
            "$Q3_TEST_ROOT/code/qcommon/unzip.c" "$Q3_TEST_ROOT/code/qcommon/md4.c" \
            "$Q3_TEST_ROOT/code/game/q_shared.c" \
            -Wl,--gc-sections -lm -o "$Q3_TEST_BIN"
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_BIN" "$Q3_TEST_DIR/install"
        # Issue #325: the real connect path (cl_parse.c, msg.c) over the same
        # install, with the executable's own DEFAULT_FS_GAME.
        Q3_TEST_DEFAULT=""
        if [[ "$Q3_TEST_BUILD" == teamarena ]]; then
            Q3_TEST_DEFAULT="$Q3_TA_DEFAULT"
            Q3_TEST_FLAGS+=("-DDEFAULT_FS_GAME=\"$Q3_TA_DEFAULT\"")
        fi
        "${CC:-cc}" -std=gnu99 -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
            -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
            -fsanitize=address,undefined -fno-sanitize=alignment "${Q3_TEST_FLAGS[@]}" \
            "-DDEFAULT_FS_GAME_EXPECTED=\"$Q3_TEST_DEFAULT\"" \
            "$Q3_TEST_ROOT/tests/cross_game_qvm_regression.c" \
            "$Q3_TEST_ROOT/code/qcommon/vm.c" "$Q3_TEST_ROOT/code/qcommon/vm_interpreted.c" \
            "$Q3_TEST_ROOT/code/qcommon/vm_static.c" \
            "$Q3_TEST_ROOT/code/qcommon/cvar.c" "$Q3_TEST_ROOT/code/qcommon/cmd.c" \
            "$Q3_TEST_ROOT/code/qcommon/msg.c" "$Q3_TEST_ROOT/code/qcommon/huffman.c" \
            "$Q3_TEST_ROOT/code/qcommon/unzip.c" "$Q3_TEST_MD4" \
            "$Q3_TEST_ROOT/code/game/q_shared.c" \
            -Wl,--gc-sections -lm -o "$Q3_TEST_BIN-cross-game"
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
            "$Q3_TEST_BIN-cross-game" "$Q3_TEST_DIR/install"
    done
done
