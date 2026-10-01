#!/usr/bin/env python3
"""Bracket the statically linked game modules in the Classic link (issue #457).

Retail 1.32c loads qagame, cgame and ui as QVMs, so every VM_Create (and
VM_Restart) starts the module from a fresh image: initialized data from the
file, everything else zero. This build links the modules into the application
instead, so the engine must restore that image itself (code/qcommon/vm_static.c,
called from Sys_LoadDll in code/mac/mac_main.c). For that it needs to know
where each module's data lives. This script provides those addresses.

  ldscript  Write the linker script: Retro68's default XCOFF script (from
            `powerpc-apple-macos-ld --verbose`) with each module archive's
            data and bss gathered first in .data and .bss and bracketed by
            q3static_<module>_{data,bss}_{start,end}. Fails when the default
            script no longer has the places the brackets go.
  check     After the link, read the linker map and fail unless every data
            and bss input section of a module archive lies inside its own
            bracket and nothing else lies inside any bracket. Also fail when
            a global symbol is defined in more than one of the module
            archives, the shared archives and the engine objects: XCOFF ld
            merges same-named uninitialized globals (as common symbols) and
            keeps one of two same-named functions without a diagnostic, so
            such a symbol would be shared across a module reset.

Code shared with the engine or another module stays out of the brackets and
is never reset. That is bg_*.c, q_shared.c and q_math.c (one copy serves the
engine and every module and holds only scratch state) and, in Team Arena,
code/ui/ui_shared.c (the Team Arena cgame uses the ui module's copy, #459).
"""
import argparse
import os
import re
import subprocess
import sys

# TOC entries, TOC data and function descriptors are written by the loader,
# never by module code, and must stay with the rest of the TOC.
DATA_EXCLUDED = {'.ds', '.tc0', '.tc', '.td'}
BSS_EXCLUDED = {'.tocbss'}
# Input sections of a module archive that hold no module state.
STATELESS = {'.pad', '.text', '.pr', '.ro', '.db', '.gl', '.xo', '.ti', '.tb',
             '.ds', '.tc0', '.tc', '.debug', '.loader'}


def fail(message):
    sys.stderr.write('static_modules.py: error: %s\n' % message)
    sys.exit(1)


def parse_modules(values):
    modules = []
    for value in values or []:
        name, sep, path = value.partition('=')
        if not sep or not re.match(r'^[a-z]+$', name) or not path:
            fail('bad --module %r (expected name=archive)' % value)
        modules.append((name, path))
    return modules


def output_section_body(script, name):
    """Return (start, end) of the body of output section `name`."""
    m = re.search(r'^[ \t]*' + re.escape(name) + r'\b[^{\n]*\{', script, re.M)
    if not m:
        return None
    depth, i = 1, m.end()
    while i < len(script) and depth:
        depth += {'{': 1, '}': -1}.get(script[i], 0)
        i += 1
    return (m.end(), i - 1) if depth == 0 else None


def input_names(body):
    names = []
    for m in re.finditer(r'\*\(\s*([^)]*?)\s*\)', body):
        names.extend(m.group(1).split())
    return names


def inject_after(script, body, anchor, text, what):
    start, end = body
    pos = script.find(anchor, start, end)
    if pos < 0:
        fail('the default linker script has no "%s" in %s; Retro68 changed its '
             'XCOFF script, so update cmake/static_modules.py' % (anchor, what))
    pos = script.index('\n', pos) + 1
    return script[:pos] + text + script[pos:]


def ldscript(args):
    text = open(args.default, encoding='latin-1').read()
    # `ld --verbose` prints the script between two lines of '='.
    parts = re.split(r'^=+\s*$', text, flags=re.M)
    script = parts[1] if len(parts) >= 3 else text
    if 'SECTIONS' not in script:
        fail('%s holds no linker script' % args.default)
    modules = parse_modules(args.module)
    if not modules:
        fail('no --module given')

    data = output_section_body(script, '.data')
    bss = output_section_body(script, '.bss')
    if not data or not bss:
        fail('the default linker script has no .data or .bss output section; '
             'Retro68 changed its XCOFF script, so update cmake/static_modules.py')
    data_names = [n for n in input_names(script[data[0]:data[1]]) if n not in DATA_EXCLUDED]
    bss_names = [n for n in input_names(script[bss[0]:bss[1]]) if n not in BSS_EXCLUDED]
    if '.rw' not in data_names or '.bss' not in bss_names or 'COMMON' not in bss_names:
        fail('the default linker script does not place .rw in .data or .bss and '
             'COMMON in .bss; update cmake/static_modules.py')

    def brackets(kind, names):
        out = ['    /* Quake III static modules (issue #457, cmake/static_modules.py):\n',
               '       each module archive\'s %s, bracketed for the reset in Sys_LoadDll. */\n' % kind]
        for name, path in modules:
            archive = os.path.basename(path)
            out.append('    . = ALIGN(16);\n')
            out.append('    q3static_%s_%s_start = .;\n' % (name, kind))
            out.append('    *%s:*(%s)\n' % (archive, ' '.join(names)))
            out.append('    . = ALIGN(16);\n')
            out.append('    q3static_%s_%s_end = .;\n' % (name, kind))
        return ''.join(out)

    # The bss anchor comes later in the file, so insert it first.
    script = inject_after(script, bss, '*(.tocbss)', brackets('bss', bss_names), '.bss')
    data = output_section_body(script, '.data')
    script = inject_after(script, data, 'PROVIDE (_data = .);', brackets('data', data_names), '.data')
    header = ('/* Generated by cmake/static_modules.py from `%s --verbose`; do not edit. */\n'
              % os.path.basename(args.ld if args.ld else 'ld'))
    with open(args.output, 'w') as f:
        f.write(header + script.strip('\n') + '\n')


def parse_map(path):
    """Yield (output section, input section, address, size, file) and collect symbols."""
    lines = open(path, encoding='latin-1').read().split('\n')
    try:
        start = next(i for i, l in enumerate(lines) if l.startswith('Linker script and memory map'))
    except StopIteration:
        fail('%s is not a linker map' % path)
    sections, symbols, loads = [], {}, []
    out, pending = None, None
    for line in lines[start:]:
        if line.startswith('LOAD '):
            loads.append(line[5:].strip())
            continue
        m = re.match(r'^(\.\w+)\s', line) or re.match(r'^(\.\w+)$', line)
        if m:
            out, pending = m.group(1), None
            continue
        m = re.match(r'^\s+0x([0-9a-f]+)\s+(q3static_\w+) = ', line)
        if m:
            symbols[m.group(2)] = int(m.group(1), 16)
            continue
        m = re.match(r'^ (\.\w+|COMMON)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+) (.+)$', line)
        if m:
            sections.append((out, m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4).strip()))
            pending = None
            continue
        m = re.match(r'^ (\.\w+|COMMON)\s*$', line)
        if m:
            pending = m.group(1)
            continue
        if pending:
            m = re.match(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+) (.+)$', line)
            if m:
                sections.append((out, pending, int(m.group(1), 16), int(m.group(2), 16), m.group(3).strip()))
            pending = None
    return sections, symbols, loads


def archive_of(file):
    m = re.match(r'^(.*)\(([^()]+)\)$', file)
    return os.path.basename(m.group(1)) if m else None


def global_definitions(nm, paths):
    """Map symbol -> set of paths that define it globally."""
    defined = {}
    for i in range(0, len(paths), 64):
        batch = paths[i:i + 64]
        try:
            out = subprocess.run([nm, '-A', '--defined-only'] + batch, check=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout.decode('latin-1')
        except (OSError, subprocess.CalledProcessError) as e:
            fail('%s failed: %s' % (nm, e))
        for line in out.split('\n'):
            m = re.match(r'^(.*?):(?:[^:\s]+:)?\s*[0-9a-fA-F]*\s+([A-Z])\s+(\S+)$', line)
            if not m or m.group(2) in 'UNW':
                continue
            path = m.group(1)
            for candidate in batch:
                if path == candidate or path.startswith(candidate + ':'):
                    path = candidate
                    break
            defined.setdefault(m.group(3), set()).add(path)
    return defined


def check(args):
    modules = parse_modules(args.module)
    shared = args.shared or []
    sections, symbols, loads = parse_map(args.map)
    ranges = {}
    for name, _ in modules:
        for kind in ('data', 'bss'):
            lo = symbols.get('q3static_%s_%s_start' % (name, kind))
            hi = symbols.get('q3static_%s_%s_end' % (name, kind))
            if lo is None or hi is None or hi < lo:
                fail('%s has no %s %s bracket; was the link run with the generated script?'
                     % (args.map, name, kind))
            ranges[(name, kind)] = (lo, hi)
    owner = {os.path.basename(path): name for name, path in modules}

    errors = []
    totals = {}
    for out, sec, addr, size, file in sections:
        if not size:
            continue
        module = owner.get(archive_of(file))
        kind = {'.data': 'data', '.bss': 'bss'}.get(out)
        if module:
            if sec in STATELESS:
                continue
            lo, hi = ranges.get((module, kind), (1, 0))
            if not (lo <= addr and addr + size <= hi):
                errors.append('%s %s at 0x%x (%d bytes) of %s is outside the %s %s bracket'
                              % (out, sec, addr, size, file, module, kind or out))
            else:
                totals[(module, kind)] = totals.get((module, kind), 0) + size
        elif kind:
            # .text, .data and .bss each start at address 0 in the XCOFF image,
            # so only sections of the same output section can overlap.
            for (name, k), (lo, hi) in ranges.items():
                if k == kind and addr < hi and lo < addr + size:
                    errors.append('%s %s at 0x%x (%d bytes) of %s lies inside the %s %s bracket'
                                  % (out, sec, addr, size, file, name, k))
    for name, _ in modules:
        if not totals.get((name, 'bss')):
            errors.append('no bss of the %s module was found in %s' % (name, args.map))

    # One definition per symbol across the module archives, the shared
    # archives and the engine's own objects.
    paths = [path for _, path in modules] + shared
    engine = [f for f in loads if f.endswith(('.obj', '.o')) and os.path.isfile(f)
              and os.path.basename(f) not in owner]
    labels = {path: name for name, path in modules}
    labels.update({path: 'shared ' + os.path.basename(path) for path in shared})
    labels.update({path: 'engine' for path in engine})
    for symbol, where in sorted(global_definitions(args.nm, paths + engine).items()):
        owners = sorted({labels[p] for p in where})
        if len(owners) > 1:
            errors.append('%s is defined in more than one module: %s' % (symbol, ', '.join(owners)))

    if errors:
        for e in errors[:50]:
            sys.stderr.write('static_modules.py: error: %s\n' % e)
        fail('%d problem(s) with the static module brackets in %s' % (len(errors), args.map))

    report = ['# Static module reset ranges (issue #457) from %s' % os.path.basename(args.map)]
    snapshot = 0
    for name, _ in modules:
        for kind in ('data', 'bss'):
            lo, hi = ranges[(name, kind)]
            report.append('%s %s 0x%08x-0x%08x %d bytes' % (name, kind, lo, hi, hi - lo))
        snapshot += ranges[(name, 'data')][1] - ranges[(name, 'data')][0]
    report.append('snapshot %d bytes' % snapshot)
    text = '\n'.join(report) + '\n'
    sys.stdout.write(text)
    if args.output:
        with open(args.output, 'w') as f:
            f.write(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = parser.add_subparsers(dest='command')
    p = sub.add_parser('ldscript')
    p.add_argument('--default', required=True, help='output of `ld --verbose`')
    p.add_argument('--ld', help='the linker that printed it')
    p.add_argument('--output', required=True)
    p.add_argument('--module', action='append', help='name=archive, in link order')
    p = sub.add_parser('check')
    p.add_argument('--map', required=True)
    p.add_argument('--nm', required=True)
    p.add_argument('--module', action='append', help='name=archive path')
    p.add_argument('--shared', action='append', help='archive shared with the engine or another module')
    p.add_argument('--output', help='file for the ranges report')
    args = parser.parse_args()
    if args.command == 'ldscript':
        ldscript(args)
    elif args.command == 'check':
        check(args)
    else:
        parser.print_help()
        sys.exit(2)


if __name__ == '__main__':
    main()
