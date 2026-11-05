#!/usr/bin/env python3
"""Boot the whole engine, headless, on the Quake III demo's real game data.

Synthetic fixtures cannot show that the bounds checks accept stock content
(#243, #244, #253). This links the engine the way CMakeLists.txt does, with
tests/real_content_sys.c where code/mac would be and a no-op GL, then runs a
scripted bot match on every map in the pak, a vid_restart and a clean quit,
and walks the menus with seeded input. Data cannot be committed, so
Q3_DEMO_PAK0 names the demo's pak0.pk3; see tests/run_real_content_tests.sh.

The pak's scratch copy gets the productid.txt the demo lacks, in a second pak,
so the engine takes the full game's path and not the demo restrictions.
"""
import concurrent.futures
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))) + '/'
CC = os.environ.get('CC', 'cc').split()
OBJCOPY = os.environ.get('OBJCOPY', 'objcopy')
JOBS = int(os.environ.get('Q3_REAL_CONTENT_JOBS', '4'))
TIME_LIMIT = int(os.environ.get('Q3_REAL_CONTENT_TIMEOUT', '280'))
FUZZ_SECONDS = 30

SCRIPT = """set bot_enable 1
set sv_maxclients 8
{maps}
echo ==== disconnect
disconnect
wait 60
echo ==== quit
quit
"""
MAP_SCRIPT = """echo ==== {name}
map {name}
wait {wait}
addbot sarge 2
addbot grunt 2
wait {wait}
"""


def fail(message):
    sys.exit('real_content_boot: ' + message)


def sources(pattern, *excluded):
    return [path for path in sorted(glob.glob(ROOT + pattern)) if os.path.basename(path) not in excluded]


def units():
    """(source, flags, module) in the groups CMakeLists.txt builds."""
    shared = [ROOT + 'code/game/' + name for name in
              ('bg_misc.c', 'bg_pmove.c', 'bg_slidemove.c', 'q_math.c', 'q_shared.c')]
    engine = (sources('code/qcommon/*.c', 'vm_x86.c', 'vm_ppc.c', 'vm_ppc_new.c', 'vm_sparc.c', 'vm_mips.c',
                      'vm_arm.c', 'vm_static.c') +
              sources('code/server/*.c', 'sv_rankings.c') + sources('code/client/*.c') +
              sources('code/renderer/*.c') + sources('code/botlib/*.c') +
              sources('code/jpeg-6/*.c', 'jmemdos.c', 'jmemansi.c', 'jmemname.c', 'jpegtran.c') +
              [ROOT + 'code/unix/unix_shared.c'])
    result = [(path, [], None) for path in engine]
    game = ['-DvmMain=Game_vmMain', '-DdllEntry=Game_dllEntry', '-DGAME_MODULE']
    result += [(path, game, None) for path in shared]
    result += [(path, game + ['-fno-common'], 'game')
               for path in sources('code/game/*.c', 'g_rankings.c', 'bg_lib.c') if path not in shared]
    result += [(path, ['-DvmMain=CGame_vmMain', '-DdllEntry=CGame_dllEntry', '-DCGAME_MODULE', '-fno-common'], 'cgame')
               for path in sources('code/cgame/*.c', 'cg_newdraw.c')]
    result += [(path, ['-DvmMain=UI_vmMain', '-DdllEntry=UI_dllEntry', '-DUI_MODULE', '-fno-common'], 'ui')
               for path in sources('code/q3_ui/*.c', 'ui_rankings.c', 'ui_rankstatus.c', 'ui_signup.c',
                                   'ui_login.c', 'ui_specifyleague.c')]
    return result


def target_flags():
    """32-bit, and C_ONLY on x86: that macro would also force little-endian swaps on PowerPC."""
    macros = subprocess.run(CC + ['-dM', '-E', '-x', 'c', os.devnull], stdout=subprocess.PIPE, text=True).stdout
    if '__x86_64__' in macros or '__i386__' in macros:
        return ['-m32', '-DC_ONLY']
    return []


def base_flags(arch):
    include = ['-I' + ROOT + path for path in
               ('', 'code', 'code/game', 'code/cgame', 'code/ui', 'code/client', 'code/server',
                'code/renderer', 'code/qcommon', 'code/botlib')]
    return CC + arch + ['-std=gnu99', '-fgnu89-inline', '-fsigned-char', '-fcommon', '-O1', '-g',
                        '-fno-strict-aliasing', '-fstack-protector-all', '-w', '-DBOTLIB', '-DQ3_STATIC',
                        '-DGAME_HARD_LINKED', '-DCGAME_HARD_LINKED', '-DUI_HARD_LINKED',
                        '-DBOTLIB_HARD_LINKED', '-DREAL_CONTENT_STOCK_PAK'] + include


def write_gl_table(path):
    """A no-op for each function pointer qgl.h declares, with the few answers GL's callers need."""
    header = open(ROOT + 'code/renderer/qgl.h', encoding='latin-1').read()
    names = set(re.findall(r'\(\s*APIENTRY\s*\*\s*(qgl\w+)\s*\)', header))
    names |= set(re.findall(r'\(\s*\*\s*(qglX\w+)\s*\)', header))
    with open(path, 'w') as out:
        out.write('static long noop(void) { return 0; }\n'
                  'static const char *string(unsigned e) { return e == 0x1F02 ? "1.2.0" :\n'
                  '    e == 0x1F03 ? "GL_EXT_texture_env_add" : "Stub"; }\n'
                  'static void integer(unsigned e, int *v) { *v = e == 0x0D33 || e == 0x0D34 ? 1024 : 0; }\n')
        specials = {'qglGetString': 'string', 'qglGetIntegerv': 'integer'}
        for name in sorted(names):
            out.write('void *%s = (void *)%s;\n' % (name, specials.get(name, 'noop')))


def compile_unit(unit, base, out):
    path, flags, module = unit
    obj = os.path.join(out, path.replace(ROOT, '').replace('/', '_') + '.o')
    result = subprocess.run(base + flags + ['-c', path, '-o', obj], stderr=subprocess.PIPE, text=True)
    if result.returncode:
        return obj, result.stderr[-800:]
    if module:
        # bracket the module, as cmake/static_modules.py does for the Mac link
        subprocess.check_call([OBJCOPY, '--set-section-flags', '.bss=alloc,load,contents',
                               '--rename-section', '.bss=modbss_' + module,
                               '--rename-section', '.data=moddata_' + module, obj])
    return obj, None


def build(work):
    arch = target_flags()
    base = base_flags(arch)
    out = os.path.join(work, 'obj')
    os.mkdir(out)
    objects = []
    with concurrent.futures.ThreadPoolExecutor(JOBS) as pool:
        for obj, error in pool.map(lambda unit: compile_unit(unit, base, out), units()):
            if error:
                fail('compile failed:\n' + error)
            objects.append(obj)
    write_gl_table(os.path.join(work, 'gl.c'))
    subprocess.check_call(CC + arch + ['-w', '-c', os.path.join(work, 'gl.c'), '-o', os.path.join(work, 'gl.o')])
    system = os.path.join(work, 'sys.o')
    subprocess.check_call(base + ['-c', ROOT + 'tests/real_content_sys.c', '-o', system])
    binary = os.path.join(work, 'q3')
    # the shim's Sys_* and FS_IdPakGame come first and win
    subprocess.check_call(CC + arch + [system] + objects + [os.path.join(work, 'gl.o'), '-lm', '-ldl',
                                                            '-Wl,--allow-multiple-definition', '-o', binary])
    return binary


def product_id():
    source = open(ROOT + 'code/qcommon/files.c', encoding='latin-1').read()
    bytes_ = [int(value, 0) for value in
              re.findall(r'\d+', re.search(r'fs_scrambledProductId\[[^\]]*\]\s*=\s*\{(.*?)\}', source, re.S).group(1))]
    seed, text = 5000, bytearray()
    for value in bytes_:
        text.append((value ^ seed) & 255)
        seed = (69069 * seed + 1) & 0xffffffff
    return bytes(text)


def install(work, pak):
    data = os.path.join(work, 'data')
    base = os.path.join(data, 'baseq3')
    os.makedirs(base)
    os.symlink(os.path.abspath(pak), os.path.join(base, 'pak0.pk3'))
    with zipfile.ZipFile(os.path.join(base, 'pak1.pk3'), 'w') as archive:
        archive.writestr('productid.txt', product_id())
    with zipfile.ZipFile(pak) as archive:
        maps = sorted(name[5:-4] for name in archive.namelist() if re.match(r'maps/[^/]+\.bsp$', name))
    if not maps:
        fail('%s has no maps' % pak)
    wait = os.environ.get('Q3_REAL_CONTENT_WAIT', '400')
    with open(os.path.join(base, 'run.cfg'), 'w') as out:
        out.write(SCRIPT.format(maps=''.join(MAP_SCRIPT.format(name=name, wait=wait) for name in maps)))
    return data, maps


def run(binary, data, seconds, extra, env=None):
    command = [binary, '+set', 'fs_basepath', data, '+set', 'fs_homepath', data, '+set', 's_initsound', '1'] + extra
    environment = dict(os.environ, **(env or {}))
    try:
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=seconds,
                                text=True, errors='replace', env=environment, cwd=data)
        return result.returncode, result.stdout
    except subprocess.TimeoutExpired as expired:
        output = expired.stdout or b''
        return None, output.decode('latin-1') if isinstance(output, bytes) else output


def bot_match(binary, data, maps):
    code, log = run(binary, data, TIME_LIMIT, ['+set', 'com_introplayed', '1', '+exec', 'run.cfg'])
    text = re.sub(r'\^[0-9]', '', log)
    problems = []
    if code != 0 or 'Sys_Quit: clean exit' not in text:
        problems.append('no clean quit (exit %s)' % code)
    for name in maps:
        if '==== %s\n' % name not in text.replace('==== %s \n' % name, '==== %s\n' % name):
            problems.append('never reached ' + name)
    for line in text.split('\n'):
        if re.search(r'Sys_Error|Unpure client|interpreting it|invalid BSP|malformed|Fatal', line) and 'chat' not in line:
            problems.append(line.strip())
    if problems:
        sys.stdout.write(log[-3000:])
        fail('bot match: ' + '; '.join(problems[:8]))
    print('bot match passed on %s: %d console lines, sv_pure on, stock modules' % (' '.join(maps), len(text.split('\n'))))


def fuzz(binary, data):
    for seed in ('1', '2'):
        config = os.path.join(data, 'baseq3', 'q3config.cfg')
        if os.path.exists(config):
            os.remove(config)
        code, log = run(binary, data, FUZZ_SECONDS, [], {'HOST_FUZZ': seed})
        if code not in (None, 0):
            sys.stdout.write(log[-2000:])
            fail('menu input seed %s exited %s' % (seed, code))
        print('menu input seed %s ran %d seconds without a fault' % (seed, FUZZ_SECONDS))


def main():
    pak = os.environ.get('Q3_DEMO_PAK0')
    if not pak or not os.path.exists(pak):
        fail('Q3_DEMO_PAK0 must name the demo pak0.pk3')
    work = tempfile.mkdtemp(prefix='q3-real-content.', dir=os.environ.get('TMPDIR', '/var/tmp'))
    try:
        binary = build(work)
        data, maps = install(work, pak)
        bot_match(binary, data, maps)
        fuzz(binary, data)
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    main()
