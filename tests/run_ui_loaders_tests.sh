#!/usr/bin/env bash
# Issue #12: the Team Arena UI's _UI_Init must run UI_BuildQ3Model_List and
# UI_LoadBots, as retail does, and both must finish within their pools and a
# time and memory budget over retail-shaped data.
#
# Builds the real Team Arena ui module (code/ui/*.c but ui_syscalls.c, with
# Q3_STATIC, UI_MODULE and MISSIONPACK as CMakeLists.txt builds ui_mp_obj),
# a weak default for every trap of code/ui/ui_syscalls.c, and
# tests/ui_loaders_regression.c, whose traps serve the real files.c, then
# runs _UI_Init over each install below:
#   retail  a retail-sized install: 24 player models with retail's icon
#           layout (icon_default, extra skins, icon_red/icon_blue), 32 bots
#           in scripts/bots.txt and 2 more in scripts/extra.bot
#   cap     40 models with 8 skins each: the head list stops at
#           MAX_PLAYERMODELS (256)
#   demo    the Quake III demo's baseq3/pak0.pk3, if $Q3_DEMO_PAK0 or
#           release_mac/content/Quake 3 Arena/baseq3/pak0.pk3 holds it; it is
#           only linked into the scratch install, never copied or committed.
# The expected heads and bots are derived here from each pk3's listing with
# retail's rules, independently of the C code.
set -euo pipefail
export TMPDIR="${TMPDIR:-/var/tmp}"

Q3_TEST_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Q3_TEST_DIR="$(mktemp -d -p "${TMPDIR:-/var/tmp}" q3-ui-loaders.XXXXXX)"
trap 'rm -rf -- "$Q3_TEST_DIR"' EXIT
Q3_CC="${CC:-cc}"
C="$Q3_TEST_ROOT/code"

Q3_DEMO="${Q3_DEMO_PAK0:-$Q3_TEST_ROOT/release_mac/content/Quake 3 Arena/baseq3/pak0.pk3}"
Q3_INSTALLS=(retail cap)
if [[ -f "$Q3_DEMO" ]]; then
    mkdir -p "$Q3_TEST_DIR/demo/baseq3"
    ln -s "$Q3_DEMO" "$Q3_TEST_DIR/demo/baseq3/pak0.pk3"
    Q3_INSTALLS+=(demo)
else
    echo "ui loaders: SKIPPED the demo pak0.pk3 pass (set Q3_DEMO_PAK0 to run it)"
fi

python3 - "$C/ui/ui_syscalls.c" "$Q3_TEST_DIR" "${Q3_INSTALLS[@]}" <<'PY_SETUP'
import os, re, sys, zipfile
syscalls, out, installs = sys.argv[1], sys.argv[2], sys.argv[3:]

# A weak default for every trap_* the module's ui_syscalls.c defines.
text = open(syscalls, encoding='latin-1').read()
text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
text = re.sub(r'//[^\n]*', '', text)
stubs = ['#include "%s"' % os.path.join(os.path.dirname(syscalls), 'ui_local.h'),
         'void Q3T_DefaultTrap( const char *name );']
seen = set()
for m in re.finditer(r'^([A-Za-z_][\w \t*]*?)\b(trap_\w+)\s*\(([^)]*)\)\s*\{', text, re.M):
    rtype, name, params = m.group(1).strip(), m.group(2), ' '.join(m.group(3).split())
    if name in seen:
        continue
    seen.add(name)
    ret = '' if rtype == 'void' else ' return (%s)0;' % rtype
    stubs.append('__attribute__((weak)) %s %s( %s ) { Q3T_DefaultTrap( "%s" );%s }'
                 % (rtype, name, params or 'void', name, ret))
if len(seen) < 50:
    sys.exit('only %d traps found in %s' % (len(seen), syscalls))
open(os.path.join(out, 'ui_traps.c'), 'w').write('\n'.join(stubs) + '\n')

def pack(install, entries):
    os.makedirs(os.path.join(out, install, 'baseq3'))
    with zipfile.ZipFile(os.path.join(out, install, 'baseq3', 'pak0.pk3'), 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('default.cfg', b'// default\n')
        for name, data in entries:
            z.writestr(name, data)

def models(names, skins):
    entries = [('models/players/', b'')]
    for model in names:
        entries.append(('models/players/%s/' % model, b''))
        for skin in ['default', 'red', 'blue'] + skins(model):
            entries.append(('models/players/%s/icon_%s.tga' % (model, skin), b'tga'))
            entries.append(('models/players/%s/%s.skin' % (model, skin), b'skin'))
    return entries

def bot(i):
    return ('{\r\nname\t\tBot%02d\r\nmodel\t\tmodel%02d\r\naifile\tbots/bot%02d_c.c\r\n}\r\n\r\n'
            % (i, i % 24, i)).encode()

for install in installs:
    if install == 'retail':
        names = ['model%02d' % i for i in range(24)]
        entries = models(names, lambda m: ['skin%d' % k for k in range(int(m[-2:]) % 4)])
        bots_txt = b''.join(bot(i) for i in range(32))
        assert len(bots_txt) < 8192	# MAX_BOTS_TEXT
        entries += [('scripts/bots.txt', bots_txt), ('scripts/extra.bot', bot(32) + bot(33))]
        pack(install, entries)
    elif install == 'cap':
        names = ['m%02d' % i for i in range(40)]
        pack(install, models(names, lambda m: ['s%d' % k for k in range(7)]) +
             [('scripts/bots.txt', bot(0))])

    # Retail's discovery, from the pk3 listing alone.
    z = zipfile.ZipFile(os.path.join(out, install, 'baseq3', 'pak0.pk3'))
    heads, bots = [], []
    for name in z.namelist():
        m = re.match(r'models/players/([^/]+)/icon_([^/]+)\.tga$', name, re.I)
        if m and m.group(2).lower() not in ('red', 'blue'):
            heads.append(m.group(1) if m.group(2).lower() == 'default' else m.group(1) + '/' + m.group(2))
        if re.match(r'scripts/[^/]+\.bot$', name, re.I) or name.lower() == 'scripts/bots.txt':
            bots += [b.decode() for b in re.findall(rb'^\s*name\s+(\S+)', z.read(name), re.M | re.I)]
    if len(heads) > 256:	# MAX_PLAYERMODELS
        heads = ['#256']
    assert heads and bots, install
    open(os.path.join(out, install + '.heads'), 'w').write(','.join(heads))
    open(os.path.join(out, install + '.bots'), 'w').write(','.join(bots))
    print('ui loaders: %s install, retail rules find %s heads and %d bots'
          % (install, heads[0][1:] if heads[0][0] == '#' else len(heads), len(bots)))
PY_SETUP

Q3_FLAGS=(-std=gnu99 -fgnu89-inline -fno-strict-aliasing -fsigned-char -fno-common
    -fno-omit-frame-pointer -ffunction-sections -fdata-sections
    -fsanitize=address,undefined -DQ3_STATIC -I"$C/qcommon")
Q3_UI=(-DUI_MODULE -DvmMain=UI_vmMain -DdllEntry=UI_dllEntry -DMISSIONPACK)
mkdir -p "$Q3_TEST_DIR/obj"
Q3_OBJECTS=()
# Module code is retail code: its warnings are not this test's business.
for f in "$C"/ui/*.c "$Q3_TEST_DIR/ui_traps.c"; do
    b="$(basename "$f" .c)"
    [[ "$b" == ui_syscalls ]] && continue
    "$Q3_CC" "${Q3_FLAGS[@]}" "${Q3_UI[@]}" -w -c "$f" -o "$Q3_TEST_DIR/obj/$b.o"
    Q3_OBJECTS+=("$Q3_TEST_DIR/obj/$b.o")
done
"$Q3_CC" "${Q3_FLAGS[@]}" "${Q3_UI[@]}" -c "$Q3_TEST_ROOT/tests/ui_loaders_regression.c" \
    -o "$Q3_TEST_DIR/obj/fixture.o"
"$Q3_CC" "${Q3_FLAGS[@]}" -DQ3_TEST_ENGINE -c "$Q3_TEST_ROOT/tests/ui_loaders_regression.c" \
    -o "$Q3_TEST_DIR/obj/engine.o"
# q_shared.c, q_math.c and bg_misc.c once, outside the module, as game_shared_mp_obj.
for f in "$C/qcommon/unzip.c" "$C/qcommon/md4.c" "$C/game/q_shared.c" "$C/game/q_math.c" "$C/game/bg_misc.c"; do
    "$Q3_CC" "${Q3_FLAGS[@]}" -DMISSIONPACK -w -c "$f" -o "$Q3_TEST_DIR/obj/shared_$(basename "$f" .c).o"
    Q3_OBJECTS+=("$Q3_TEST_DIR/obj/shared_$(basename "$f" .c).o")
done
"$Q3_CC" -fsanitize=address,undefined "$Q3_TEST_DIR/obj/fixture.o" "$Q3_TEST_DIR/obj/engine.o" \
    "${Q3_OBJECTS[@]}" -Wl,--gc-sections -lm -o "$Q3_TEST_DIR/ui_loaders"

for Q3_INSTALL in "${Q3_INSTALLS[@]}"; do
    # LeakSanitizer cannot initialize in the local ptrace sandbox; the
    # fixture checks its own time and memory budgets, timeout catches a hang.
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        timeout 120 "$Q3_TEST_DIR/ui_loaders" "$Q3_TEST_DIR/$Q3_INSTALL" \
        "$(cat "$Q3_TEST_DIR/$Q3_INSTALL.heads")" "$(cat "$Q3_TEST_DIR/$Q3_INSTALL.bots")" \
        > "$Q3_TEST_DIR/$Q3_INSTALL.log" 2>&1 || { cat "$Q3_TEST_DIR/$Q3_INSTALL.log"; exit 1; }
    grep -v "^UI_Init\|^===\|^\*\*\*\|^UI_LoadMenus\|^UI_Refresh" "$Q3_TEST_DIR/$Q3_INSTALL.log" | sed "s/^/$Q3_INSTALL: /"
done
