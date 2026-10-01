#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   depdrift — link dependencies a recipe does not declare
# ---------------------------------
#
# Run from the repository root over a built tree:
#
#   python3 testing/depdrift.py [--root build/fs] [--repo <repo>]... [--strict]
#
# Every package in <root>/var/lib/kpkg/db owns the paths its manifest lists.
# For each package, every ELF64 file it owns is read for its DT_NEEDED sonames
# and every .pc file for its Requires and Requires.private modules; a soname
# resolves under usr/lib, lib and usr/local/lib, a module under the pkgconfig
# directories, and the path found names its owning package. An owner outside
# the package's declared `depends` closure is a dependency the recipe uses but
# does not declare.
#
# The serial build hides such a dependency whenever its owner happens to be
# installed first; a build that runs one phase's ports concurrently does not.
# So every drift is classified by where the owner is installed, using the
# phase lists resolved the way testing/phaseclosure.py resolves them:
#
#   same phase    installed by the package's own phase, outside its order run.
#                 Only the serial position made it present: ACTIONABLE.
#   order run     installed by the package's phase from its order run.
#   earlier phase installed by an earlier phase, so present either way.
#   unplaced      the owner or the package is in no phase list.
#
# Declared closures reuse phaseclosure's Resolver and read_depends, so a name
# resolves over the same PORT_REPO a phase uses; --repo replaces that with an
# explicit list for every package. Exit status is 0 unless --strict is given
# and an actionable drift was found. It is advisory: preflight does not run
# it, because it needs a built tree.

import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from phaseclosure import (Resolver, read_depends, read_list,  # noqa: E402
                          read_repos)

LIB_DIRS = ('usr/lib', 'lib', 'usr/local/lib', 'usr/lib64', 'lib64')
PC_DIRS = ('usr/lib/pkgconfig', 'usr/share/pkgconfig', 'usr/local/lib/pkgconfig')
DT_NEEDED, DT_STRTAB = 1, 5
PT_LOAD, PT_DYNAMIC = 1, 2


def elf_needed(path):
    """DT_NEEDED of an ELF64 file through its program headers, [] otherwise."""
    try:
        with open(path, 'rb') as f:
            head = f.read(64)
            if len(head) < 64 or head[:4] != b'\x7fELF' or head[4] != 2:
                return []
            e = '<' if head[5] == 1 else '>'
            phoff, = struct.unpack_from(e + 'Q', head, 32)
            phentsize, phnum = struct.unpack_from(e + 'HH', head, 54)
            f.seek(phoff)
            ph = f.read(phentsize * phnum)
            loads, dyn = [], None
            for i in range(phnum):
                t, _, off, vaddr, _, filesz = struct.unpack_from(
                    e + 'IIQQQQ', ph, i * phentsize)
                if t == PT_LOAD:
                    loads.append((vaddr, off, filesz))
                elif t == PT_DYNAMIC:
                    dyn = (off, filesz)
            if not dyn:
                return []
            f.seek(dyn[0])
            raw = f.read(dyn[1])
            needed, strtab = [], None
            for i in range(0, len(raw) - 15, 16):
                tag, val = struct.unpack_from(e + 'qQ', raw, i)
                if tag == 0:
                    break
                if tag == DT_NEEDED:
                    needed.append(val)
                elif tag == DT_STRTAB:
                    strtab = val
            if strtab is None:
                return []
            base = None
            for vaddr, off, filesz in loads:
                if vaddr <= strtab < vaddr + filesz:
                    base = strtab - vaddr + off
            if base is None:
                return []
            out = []
            for n in needed:
                f.seek(base + n)
                s = f.read(256).split(b'\0', 1)[0]
                out.append(s.decode('utf-8', 'replace'))
            return out
    except (OSError, struct.error):
        return []


def pc_requires(path):
    """Module names on a .pc file's Requires and Requires.private lines."""
    mods = []
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            for line in f:
                key, sep, val = line.partition(':')
                if not sep or key.strip() not in ('Requires', 'Requires.private'):
                    continue
                toks = val.replace(',', ' ').split()
                skip = False
                for t in toks:
                    if skip:
                        skip = False
                    elif t[0] in '<>=!':
                        skip = True
                    elif '$' not in t:
                        mods.append(t)
    except OSError:
        pass
    return mods


def read_db(root):
    """{package: [paths]} and {path: owner}, paths without ./ or a trailing /."""
    db = os.path.join(root, 'var', 'lib', 'kpkg', 'db')
    files, owner = {}, {}
    for name in sorted(os.listdir(db)):
        p = os.path.join(db, name)
        if name.startswith('.') or not os.path.isfile(p):
            continue
        with open(p, encoding='utf-8', errors='replace') as f:
            lines = f.read().split('\n')[1:]
        paths = []
        for ln in lines:
            if not ln or ln.endswith('/'):
                continue
            ln = ln[2:] if ln.startswith('./') else ln
            paths.append(ln)
            owner.setdefault(ln, name)
        files[name] = paths
    return files, owner


def phase_places(repo_root):
    """{port: (phase index, serial position, in order run)} and phase repos."""
    base = os.path.join(repo_root, 'script', 'phases')
    res = Resolver()
    places, phase_repos, phase_names = {}, [], []
    for d in sorted(os.listdir(base)):
        pd = os.path.join(base, d)
        txt, dd = os.path.join(pd, 'packages.txt'), os.path.join(pd, 'packages.d')
        if os.path.isfile(txt):
            names = read_list([txt], False)
        elif os.path.isdir(dd):
            names = read_list([os.path.join(dd, f) for f in sorted(os.listdir(dd))
                               if f.endswith('.txt') and not f.startswith('.')], True)
        else:
            continue
        _, repos = read_repos(repo_root, pd)
        idx = len(phase_repos)
        phase_repos.append(repos)
        phase_names.append(d)
        visited, pos = set(), [0]

        def emit(n, run):
            if n not in places:
                places[n] = (idx, pos[0], run)
                pos[0] += 1

        def walk(n, run):
            pdir = res.find(repos, n)
            if not pdir:
                return
            for dep in read_depends(pdir):
                if dep in visited or dep in places:
                    continue
                visited.add(dep)
                walk(dep, run)
                emit(dep, run)

        for n, _, _, in_run in names:
            if n in visited or n in places:
                continue
            visited.add(n)
            walk(n, in_run)
            emit(n, in_run)
    return places, phase_repos, phase_names, res


def closure(res, repos, name):
    seen, stack = set(), [name]
    while stack:
        n = stack.pop()
        pdir = res.find(repos, n)
        if not pdir:
            continue
        for dep in read_depends(pdir):
            if dep not in seen:
                seen.add(dep)
                stack.append(dep)
    return seen


def main():
    ap = argparse.ArgumentParser(description='undeclared link dependencies in a built tree')
    ap.add_argument('--root', default='build/fs')
    ap.add_argument('--repo', action='append', default=[],
                    help='port repository for every closure (repeatable); '
                         'default: each phase\'s PORT_REPO')
    ap.add_argument('--strict', action='store_true',
                    help='exit 1 when a same-phase drift is found')
    a = ap.parse_args()

    repo_root = os.getcwd()
    if not os.path.isdir(os.path.join(repo_root, 'script', 'phases')):
        print('depdrift: no script/phases here; run from the repository root')
        return 1
    root = a.root
    files, owner = read_db(root)
    places, phase_repos, phase_names, res = phase_places(repo_root)
    fixed = [os.path.abspath(r) for r in a.repo]

    def lib_owner(soname):
        for d in LIB_DIRS:
            o = owner.get(d + '/' + soname)
            if o:
                return o
        return None

    def pc_owner(mod):
        for d in PC_DIRS:
            o = owner.get(d + '/' + mod + '.pc')
            if o:
                return o
        return None

    counts = {'same phase': 0, 'order run': 0, 'earlier phase': 0, 'unplaced': 0}
    actionable = []
    other = []
    unowned = set()
    for pkg in sorted(files):
        used = {}
        for rel in files[pkg]:
            full = os.path.join(root, rel)
            if os.path.islink(full) or not os.path.isfile(full):
                continue
            if rel.endswith('.pc'):
                for m in pc_requires(full):
                    o = pc_owner(m)
                    if o:
                        used.setdefault(o, f'{rel} requires {m}')
                continue
            for so in elf_needed(full):
                o = lib_owner(so)
                if o:
                    used.setdefault(o, f'{rel} needs {so}')
                else:
                    unowned.add(so)
        used.pop(pkg, None)
        if not used:
            continue
        place = places.get(pkg)
        repos = fixed or (phase_repos[place[0]] if place else [r for p in phase_repos for r in p])
        declared = closure(res, repos, pkg)
        for o in sorted(used):
            if o in declared:
                continue
            op = places.get(o)
            if not place or not op:
                kind = 'unplaced'
            elif op[0] < place[0]:
                kind = 'earlier phase'
            elif op[0] == place[0] and op[2]:
                kind = 'order run'
            elif op[0] == place[0]:
                kind = 'same phase'
            else:
                kind = 'unplaced'
            counts[kind] += 1
            where = phase_names[place[0]] if place else '-'
            line = f'{pkg} -> {o}  [{where}] ({used[o]})'
            if kind == 'same phase':
                when = 'earlier' if op[1] < place[1] else 'LATER'
                actionable.append(f'{line}; installed {when} in the same phase')
            else:
                other.append(f'{kind}: {line}')

    for s in actionable:
        print('DRIFT ' + s)
    for s in other:
        print('note  ' + s)
    print(f'depdrift: {len(files)} packages; undeclared owners: '
          + ', '.join(f'{k} {v}' for k, v in counts.items())
          + f'; {len(unowned)} soname(s) owned by no package')
    return 1 if a.strict and actionable else 0


if __name__ == '__main__':
    sys.exit(main())
