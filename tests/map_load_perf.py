#!/usr/bin/env python3
"""Time map loading and box queries on real maps, and check outputs against other trees (#251).

Game data cannot be committed, so this runs only when Q3_RETAIL_BASEQ3 names a
baseq3 with the retail paks or Q3_DEMO_PAK0 names the demo's pak0.pk3, as
tests/real_content_boot.py does; tests/run_map_load_perf_tests.sh skips it
otherwise. The paks are only read: the wanted maps' .bsp and .aas are copied
into a fresh scratch directory.

tests/map_load_perf.c and tests/map_load_perf_renderer.c are built against
this tree and against each git ref in Q3_PERF_REFERENCES, and run on each map
in Q3_PERF_MAPS (default q3dm1 q3dm7) at each level in Q3_PERF_OPT (default
-O2). Each mode prints a best and median time, and a hash of its outputs that
holds no pointers. A reference's hash must match this tree's, unless the ref
is written ref:timing, for a tree whose outputs may differ. Each mode runs
Q3_PERF_ROUNDS times (default 3), each round starting from a different tree,
and keeps each tree's best; the run orders are printed.

    Q3_RETAIL_BASEQ3=... Q3_PERF_REFERENCES='origin/master 204fe36:timing' \\
        Q3_PERF_OPT='-O2 -O0' python3 tests/map_load_perf.py
"""
import concurrent.futures
import glob
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = os.environ.get('CC', 'cc').split()
JOBS = int(os.environ.get('Q3_PERF_JOBS', '4'))
BOXES = os.environ.get('Q3_PERF_BOXES', '200000')
# mode, arguments after the map
MODES = [('cm', ['15']), ('box', [BOXES, '5']), ('aas', ['5']), ('rload', ['9']), ('rbox', [BOXES, '5'])]


def fail(message):
    sys.exit('map_load_perf: ' + message)


def extract_maps(work, wanted):
    data = os.path.join(work, 'data')
    os.mkdir(data)
    retail = os.environ.get('Q3_RETAIL_BASEQ3')
    paks = sorted(glob.glob(os.path.join(retail, 'pak?.pk3'))) if retail else [os.environ['Q3_DEMO_PAK0']]
    found = set()
    for path in paks:
        with zipfile.ZipFile(path) as archive:
            for name in archive.namelist():
                match = re.match(r'maps/([^/]+)\.(bsp|aas)$', name)
                if match and match.group(1) in wanted:
                    with open(os.path.join(data, os.path.basename(name)), 'wb') as out:
                        out.write(archive.read(name))
                    found.add(match.group(1))
    maps = [name for name in wanted if os.path.exists(os.path.join(data, name + '.bsp'))
            and os.path.exists(os.path.join(data, name + '.aas'))]
    if not maps:
        fail('none of %s in %s' % (' '.join(wanted), ' '.join(paks)))
    return data, maps


def export_tree(work, ref):
    tree = os.path.join(work, 'tree-' + re.sub(r'[^A-Za-z0-9._-]', '_', ref))
    os.mkdir(tree)
    archive = subprocess.run(['git', '-C', ROOT, 'archive', '--format=tar', ref, 'code'],
                             stdout=subprocess.PIPE, check=True).stdout
    path = tree + '.tar'
    with open(path, 'wb') as out:
        out.write(archive)
    with tarfile.open(path) as tar:
        tar.extractall(tree, filter='data')
    os.remove(path)
    return tree


def target_flags():
    """32-bit like the Mac, and C_ONLY on x86 as in real_content_boot.py; botlib's 8-byte
    block headers also break the 16-byte alignment 64-bit code assumes."""
    macros = subprocess.run(CC + ['-dM', '-E', '-x', 'c', os.devnull], stdout=subprocess.PIPE, text=True).stdout
    if '__x86_64__' in macros or '__i386__' in macros:
        # SSE arithmetic, as PowerPC has no x87 excess precision to vary with optimization
        return ['-m32', '-DC_ONLY', '-msse2', '-mfpmath=sse']
    return []


def build(tree, out, opt):
    retail = 'TryGetMemory' not in open(os.path.join(tree, 'code/game/botlib.h'), encoding='latin-1').read()
    flags = CC + target_flags() + ['-std=gnu99', '-fgnu89-inline', '-fno-strict-aliasing', '-w', opt, '-DBOTLIB']
    if retail:
        flags.append('-DQ3_PERF_RETAIL')
    objects = os.path.join(out + '.obj')
    os.mkdir(objects)
    sources = [os.path.join(tree, 'code', path) for path in
               ('qcommon/cm_load.c', 'qcommon/cm_patch.c', 'qcommon/cm_polylib.c', 'qcommon/cm_test.c',
                'qcommon/cm_trace.c', 'qcommon/md4.c', 'game/q_shared.c', 'game/q_math.c')]
    sources += sorted(glob.glob(os.path.join(tree, 'code/botlib/*.c')))
    units = [(path, []) for path in sources]
    units.append((os.path.join(ROOT, 'tests/map_load_perf.c'),
                  ['-I' + os.path.join(tree, 'code', part) for part in ('qcommon', 'botlib', 'game')]))
    units.append((os.path.join(ROOT, 'tests/map_load_perf_renderer.c'),
                  ['-ffunction-sections', '-fdata-sections', '-I' + os.path.join(tree, 'code/renderer')]))

    def compile_unit(unit):
        path, extra = unit
        obj = os.path.join(objects, os.path.basename(path) + '.o')
        result = subprocess.run(flags + extra + ['-c', path, '-o', obj], stderr=subprocess.PIPE, text=True)
        return obj, result.stderr[-1500:] if result.returncode else None

    with concurrent.futures.ThreadPoolExecutor(JOBS) as pool:
        results = list(pool.map(compile_unit, units))
    for obj, error in results:
        if error:
            fail('compile failed in %s:\n%s' % (tree, error))
    subprocess.run(CC + target_flags() + ['-Wl,--gc-sections'] + [obj for obj, _ in results] + ['-lm', '-o', out],
                   check=True)
    shutil.rmtree(objects)


def run(binary, data, name, mode, args):
    result = subprocess.run([binary, mode, data, name] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True)
    if result.returncode:
        fail('%s %s %s failed:\n%s' % (os.path.basename(binary), mode, name, result.stderr[-1500:]))
    times, hashes, info = {}, {}, []
    for line in result.stdout.splitlines():
        part = line.split()
        if part[0] == 'time':
            times[part[1]] = (float(part[2]), float(part[3]))
        elif part[0] == 'hash':
            hashes[part[1]] = part[2]
        elif part[0] == 'info':
            info.append(' '.join(part[1:]))
    return times, hashes, info


def main():
    if not os.environ.get('Q3_RETAIL_BASEQ3') and not os.environ.get('Q3_DEMO_PAK0'):
        fail('Q3_RETAIL_BASEQ3 must name a retail baseq3, or Q3_DEMO_PAK0 the demo pak0.pk3')
    wanted = os.environ.get('Q3_PERF_MAPS', 'q3dm1 q3dm7').split()
    references = os.environ.get('Q3_PERF_REFERENCES', '').split()
    opts = os.environ.get('Q3_PERF_OPT', '-O2').split()
    rounds = max(1, int(os.environ.get('Q3_PERF_ROUNDS', '3')))
    work = tempfile.mkdtemp(prefix='q3-map-load-perf.', dir=os.environ.get('TMPDIR', '/var/tmp'))
    try:
        data, maps = extract_maps(work, wanted)
        trees = [('this tree', ROOT, False)]
        for entry in references:
            ref, _, kind = entry.partition(':')
            trees.append((ref, export_tree(work, ref), kind == 'timing'))
        differences = []
        for opt in opts:
            binaries = []
            for index, (label, tree, timing_only) in enumerate(trees):
                binary = os.path.join(work, 'perf-%d%s' % (index, opt))
                build(tree, binary, opt)
                binaries.append(binary)
            for name in maps:
                print('== %s %s (ms, best/median)' % (name, opt))
                print('%-28s' % 'measure' + ''.join('%22s' % label[:21] for label, _, _ in trees))
                for mode_index, (mode, args) in enumerate(MODES):
                    # Several rounds, each starting from a different tree, so
                    # cache, clock or thermal state cannot favour one tree in
                    # every measure; each tree keeps its best over the rounds.
                    results = [None] * len(binaries)
                    orders = []
                    for round_index in range(rounds):
                        start = (round_index + mode_index) % len(binaries)
                        order = [(start + k) % len(binaries) for k in range(len(binaries))]
                        orders.append(order)
                        for index in order:
                            outcome = run(binaries[index], data, name, mode, args)
                            if results[index] is None:
                                results[index] = outcome
                            else:
                                times = results[index][0]
                                for measure, (best, median) in outcome[0].items():
                                    old_best, old_median = times.get(measure, (best, median))
                                    times[measure] = (min(best, old_best), min(median, old_median))
                    print('  %s: run order %s' % (mode, ' | '.join(
                        ', '.join(trees[index][0] for index in order) for order in orders)))
                    for measure in results[0][0]:
                        cells = []
                        for times, _, _ in results:
                            best, median = times.get(measure, (float('nan'), float('nan')))
                            cells.append('%10.2f/%-10.2f' % (best, median))
                        print('%-28s' % measure + ''.join('%22s' % cell for cell in cells))
                    for line in results[0][2]:
                        print('  this tree: ' + line)
                    for (label, _, timing_only), (_, hashes, _) in zip(trees[1:], results[1:]):
                        for measure, value in results[0][1].items():
                            other = hashes.get(measure)
                            if other is not None and other != value:
                                note = '%s %s %s: %s differs from this tree' % (name, opt, measure, label)
                                print(('  timing only, ' if timing_only else '  DIFFERS: ') + note)
                                if not timing_only:
                                    differences.append(note)
                            elif other is not None:
                                print('  identical to %s: %s' % (label, measure))
        if differences:
            fail('outputs differ:\n  ' + '\n  '.join(differences))
        print('map load timing done on %s' % ' '.join(maps))
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    main()
