#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   phaseclosure — every package phase installs exactly the ports its list names
# ---------------------------------
#
# Run from the repository root. For each package phase under script/phases/, in
# sorted order, the list (packages.txt, or packages.d/*.txt in byte order read
# as one file) is resolved the way kpkgdepends resolves it: depth first over
# each recipe's `depends =`, skipping what an earlier phase installed. From
# phase 30 on, the set that resolution installs must equal the list's own names
# minus what an earlier phase installed. A port a phase installs without naming
# it is reported with the phase that installs it and the later phase that names
# it, if one does.
#
# A name an earlier phase installed is a re-list: kpkg skips it while its
# recipe hash is current and rebuilds it in this phase when the recipe has
# changed. A re-list is accepted only in the list's order run — packages.d's
# 00-order.txt, or the names a packages.txt gives ahead of its first shelf
# banner (`# <shelf> — <description>`). Anywhere else it is refused with the
# phase that installs the port, so a shelf section cannot claim a port another
# phase builds.
#
# Also refused: a phase with both packages.txt and packages.d/, a name that is
# not a port on the phase's PORT_REPO, a name listed twice in one phase, and a
# dependency that resolves only on a later phase's PORT_REPO — that port cannot
# be built where the phase needs it.
#
# Names resolve as libkpkg resolves them across repositories: each PORT_REPO
# entry in order, as <repo>/<name>/ and then <repo>/<shelf>/<name>/, the first
# repository with the name winning. Within one repository this takes the first
# path where libkpkg refuses a name found at two; preflight's ports layout
# check is what refuses that case. /ports/ maps to ports/ and /kdos/ to the repository root, as the
# chroot binds them. A phase.env that sets no PORT_REPO gets kpkg.conf's
# default, /ports/core.
#
# Exit status is 1 when anything is refused. Every refusal is printed before
# the per-phase summary, so the first lines of the output name the problem.

import os
import re
import sys

EQUALITY_FROM = 30
DEFAULT_REPO = '/ports/core'
NAME_RE = re.compile(r'^[A-Za-z0-9][A-Za-z0-9._+-]*$')
SHELF_BANNER_RE = re.compile(r'^# [a-z0-9][a-z0-9-]* — ')
ORDER_FILE = '00-order.txt'
REPO_RE = re.compile(r'^\s*(?:export\s+)?PORT_REPO=(["\']?)(.*?)\1\s*(?:#.*)?$')

sys.setrecursionlimit(100000)


def read_repos(root, phase_dir):
    env = os.path.join(phase_dir, 'phase.env')
    value = DEFAULT_REPO
    try:
        with open(env, encoding='utf-8') as f:
            for line in f:
                m = REPO_RE.match(line.rstrip('\n'))
                if m:
                    value = m.group(2)
    except OSError:
        pass
    repos = []
    for r in value.split():
        if r.startswith('/ports/'):
            repos.append(os.path.join(root, 'ports', r[len('/ports/'):]))
        elif r == '/ports':
            repos.append(os.path.join(root, 'ports'))
        elif r.startswith('/kdos/'):
            repos.append(os.path.join(root, r[len('/kdos/'):]))
        else:
            repos.append(os.path.join(root, r.lstrip('/')))
    return value, repos


class Resolver:
    """Name -> port directory over one PORT_REPO, cached per repo."""

    def __init__(self):
        self.index = {}

    def _repo_index(self, repo):
        if repo in self.index:
            return self.index[repo]
        loose, shelved = {}, {}
        try:
            entries = sorted(os.listdir(repo))
        except OSError:
            entries = []
        for e in entries:
            if e.startswith('.'):
                continue
            d = os.path.join(repo, e)
            if not os.path.isdir(d):
                continue
            if os.path.isfile(os.path.join(d, 'kpkgbuild')):
                loose[e] = d
                continue
            for n in sorted(os.listdir(d)):
                p = os.path.join(d, n)
                if not n.startswith('.') and os.path.isfile(os.path.join(p, 'kpkgbuild')):
                    shelved.setdefault(n, p)
        self.index[repo] = (loose, shelved)
        return self.index[repo]

    def find(self, repos, name):
        for r in repos:
            loose, shelved = self._repo_index(r)
            if name in loose:
                return loose[name]
            if name in shelved:
                return shelved[name]
        return None


_deps_cache = {}


def read_depends(portdir):
    """kp_depends: the first `depends =` line, split on spaces only."""
    if portdir in _deps_cache:
        return _deps_cache[portdir]
    out = []
    try:
        with open(os.path.join(portdir, 'kpkgbuild'), encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.rstrip('\n')
                if not line.startswith('depends'):
                    continue
                p = line[7:].lstrip(' \t')
                if not p.startswith('='):
                    continue
                out = [t for t in p[1:].lstrip(' \t').split(' ') if t]
                break
    except OSError:
        pass
    _deps_cache[portdir] = out
    return out


def read_list(files, split):
    """kbuild_packages: one name per line, trimmed; blank and # lines skipped.

    Each name also carries whether it is in the order run: every name of a
    packages.d/00-order.txt, or of a packages.txt the names ahead of its first
    shelf banner."""
    names = []
    for path in files:
        in_run = os.path.basename(path) == ORDER_FILE if split else True
        with open(path, encoding='utf-8') as f:
            for n, line in enumerate(f, 1):
                s = line.strip()
                if not split and SHELF_BANNER_RE.match(s):
                    in_run = False
                if s and not s.startswith('#'):
                    names.append((s, path, n, in_run))
    return names


def main():
    root = os.getcwd()
    base = os.path.join(root, 'script', 'phases')
    if not os.path.isdir(base):
        print('phaseclosure: no script/phases here; run from the repository root')
        return 1

    phases = []
    fails = []
    for d in sorted(os.listdir(base)):
        pd = os.path.join(base, d)
        if not os.path.isdir(pd):
            continue
        txt = os.path.join(pd, 'packages.txt')
        dd = os.path.join(pd, 'packages.d')
        has_txt, has_dir = os.path.isfile(txt), os.path.isdir(dd)
        if has_txt and has_dir:
            fails.append(f'{d}: has both packages.txt and packages.d/ — a phase uses one')
            continue
        if has_txt:
            files = [txt]
        elif has_dir:
            files = [os.path.join(dd, f) for f in sorted(os.listdir(dd))
                     if f.endswith('.txt') and not f.startswith('.')
                     and os.path.isfile(os.path.join(dd, f))]
        else:
            continue
        m = re.match(r'^(\d+)_', d)
        num = int(m.group(1)) if m else -1
        repo_text, repos = read_repos(root, pd)
        phases.append({'dir': d, 'num': num, 'files': files, 'repos': repos,
                       'repo_text': repo_text, 'split': has_dir})

    res = Resolver()
    for i, ph in enumerate(phases):
        ph['names'] = read_list(ph['files'], ph['split'])
        ph['named'] = set(n for n, _, _, _ in ph['names'])

    def later_home(i, name):
        for ph in phases[i + 1:]:
            if res.find(ph['repos'], name):
                return ph['dir']
        return None

    def later_naming(i, name):
        return [ph['dir'] for ph in phases[i + 1:] if name in ph['named']]

    installed = {}          # port -> phase that installs it
    summary = []
    for i, ph in enumerate(phases):
        d = ph['dir']
        seen_names = {}
        order = []
        relisted = 0
        for name, path, line, in_run in ph['names']:
            where = os.path.relpath(path, root) + f':{line}'
            if not NAME_RE.match(name):
                fails.append(f'{d}: {where}: "{name}" is not a package name')
                continue
            if name in seen_names:
                fails.append(f'{d}: {where}: {name} is listed twice (first at {seen_names[name]})')
                continue
            seen_names[name] = where
            if not res.find(ph['repos'], name):
                home = later_home(i, name)
                if home:
                    fails.append(f'{d}: {where}: {name} is a port only on the PORT_REPO of '
                                 f'{home}, not on this phase\'s ({ph["repo_text"]})')
                else:
                    fails.append(f'{d}: {where}: {name} has no port')
                continue
            if name in installed:
                relisted += 1
                if ph['num'] >= EQUALITY_FROM and not in_run:
                    fails.append(f'{d}: {where}: {name} is installed by {installed[name]}; '
                                 f'a later phase may re-name it only in its order run')
            order.append(name)

        # kp_resolve: depth first, an installed port neither emitted nor walked.
        visited = set()
        emitted = []
        emitted_set = set()
        puller = {}

        def walk(n):
            pdir = res.find(ph['repos'], n)
            if not pdir:
                return
            for dep in read_depends(pdir):
                if dep in visited or dep in installed:
                    continue
                visited.add(dep)
                puller.setdefault(dep, n)
                if not res.find(ph['repos'], dep):
                    home = later_home(i, dep)
                    if home:
                        fails.append(f'{d}: {n} depends on {dep}, which is a port only on the '
                                     f'PORT_REPO of {home}, not on this phase\'s ({ph["repo_text"]})')
                    else:
                        fails.append(f'{d}: {n} depends on {dep}, which has no port')
                    continue
                walk(dep)
                if dep not in emitted_set:
                    emitted.append(dep)
                    emitted_set.add(dep)

        for n in order:
            if n in visited or n in installed:
                continue
            visited.add(n)
            walk(n)
            if n not in emitted_set:
                emitted.append(n)
                emitted_set.add(n)

        new_named = set(order) - set(installed)
        if ph['num'] >= EQUALITY_FROM:
            for p in emitted:
                if p in new_named:
                    continue
                chain = [p]
                while chain[-1] in puller and chain[-1] not in new_named and len(chain) < 12:
                    chain.append(puller[chain[-1]])
                via = ' <- '.join(chain)
                later = later_naming(i, p)
                if later:
                    fails.append(f'{p}: installed by {d} (pulled in: {via}) but named by '
                                 f'{", ".join(later)}')
                else:
                    fails.append(f'{p}: installed by {d} (pulled in: {via}) but not named '
                                 f'in its list')
        for p in emitted:
            installed[p] = d
        summary.append(f'{d}: {len(ph["names"])} names, installs {len(emitted)}'
                       + (f', re-lists {relisted}' if relisted else '')
                       + ('' if ph['num'] >= EQUALITY_FROM else ' (closure not checked below '
                          f'{EQUALITY_FROM})'))

    for f in fails:
        print('FAIL ' + f)
    for s in summary:
        print(s)
    if fails:
        print(f'phaseclosure: {len(fails)} problem(s)')
        return 1
    print('phaseclosure: every package phase installs exactly the ports its list names')
    return 0


if __name__ == '__main__':
    sys.exit(main())
