#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   linkcheck — run-time references a built tree cannot satisfy
# ---------------------------------
#
# Run from the repository root over a built tree:
#
#   python3 testing/linkcheck.py [--root build/fs] [--all]
#
# Every file a package's manifest in var/lib/kpkg/db names is read once.
#
# FAIL  undeclared  An ELF file's DT_NEEDED resolves, the way musl's loader
#                   resolves it (the object's RPATH/RUNPATH with $ORIGIN, then
#                   etc/ld-musl-x86_64.path or the default /lib:/usr/local/lib:
#                   /usr/lib), to a file owned by a package outside the
#                   consumer's transitive `depends`. musl, gcc and
#                   linux-headers are exempt: every port has them. A
#                   dependency only a build order supplied is one a removal,
#                   a parallel build or an update can take away.
# FAIL  interpreter An executable script's `#!` interpreter, or an ELF
#                   file's PT_INTERP, names a path under /usr (or /bin, /sbin,
#                   /lib, which are /usr here) that does not resolve to an
#                   executable in the root; or `#!/usr/bin/env NAME` names a
#                   command no PATH directory of the root holds.
# FAIL  openssl     One process would map libcrypto.so.3 and libcrypto.so.4.
#                   musl ignores symbol versions, so the second library's
#                   symbols bind to whichever was mapped first. The sets read
#                   are each ELF object's own NEEDED closure; each Qt 5 plugin
#                   with libQt5Network.so.5, the library its host programs
#                   link; each Python extension module with _hashlib and
#                   _ssl, which nearly every Python program imports. The
#                   extension modules of a package in PY_OPENSSL_ACCEPTED
#                   print WARN openssl for that pairing instead of failing:
#                   known-gaps.md names the crash a program importing both
#                   can meet.
# WARN  python      A `#!` naming bare `python` (env or /usr/bin/python):
#                   which Python it means is the PATH's choice, not the
#                   script's. python3 is the name to use.
# WARN  unresolved  A DT_NEEDED nothing in the root provides on the object's
#                   own search path. A plugin can still load when its host
#                   has the library mapped already, so this is not a failure.
# WARN  unowned     A DT_NEEDED resolves to a file no package owns.
#
# FAIL lines and the openssl, python and unowned warnings are printed; the unresolved
# warnings are counted, and listed with --all.
#
# Declared closures reuse depdrift's phase placement and phaseclosure's
# Resolver, so a name resolves over the same PORT_REPO the package's phase
# uses. Exit status 1 when a FAIL line was printed, 0 otherwise, 2 when the
# root or the repository cannot be read. 70_image runs it (087_gates.sh).

import argparse
import os
import stat
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import treescan  # noqa: E402
from depdrift import closure, phase_places  # noqa: E402

EXEMPT = {'musl', 'gcc', 'linux-headers'}
DEFAULT_LD = ('lib', 'usr/local/lib', 'usr/lib')
EXEC_ROOTS = ('usr/', 'bin/', 'sbin/', 'lib/', 'lib64/')
PATH_DIRS = ('usr/local/sbin', 'usr/local/bin', 'usr/sbin', 'usr/bin')
CRYPTO = ('libcrypto.so.3', 'libcrypto.so.4')
# PyQt5's QtNetwork and the modules linking libQt5Network map OpenSSL 3 and
# are shipped on purpose. Their own NEEDED closures are still checked.
PY_OPENSSL_ACCEPTED = {'python3-pyqt5'}


class Tree:
    def __init__(self, root):
        self.root = root
        self.aliases = treescan.canon_load(root)
        self.files, self.owner = treescan.read_db(root, self.aliases)
        self.elf = {}
        self.ld = self._ld_path()
        self.closures = {}

    def _ld_path(self):
        for name in ('etc/ld-musl-x86_64.path',):
            try:
                with open(os.path.join(self.root, name), encoding='utf-8') as f:
                    dirs = [d.strip().lstrip('/') for d in f.read().replace(':', '\n').split('\n')]
                return tuple(d for d in dirs if d)
            except OSError:
                pass
        return DEFAULT_LD

    def parse(self, rel):
        if rel not in self.elf:
            full = os.path.join(self.root, rel)
            self.elf[rel] = treescan.Elf(full) if treescan.is_elf(full) else None
        return self.elf[rel]

    def find_lib(self, rel, elf, soname):
        """(path as searched, real path) for soname needed by rel, or None."""
        if '/' in soname:
            r = treescan.lookup(self.root, soname.lstrip('/'))
            return (soname.lstrip('/'), r) if r else None
        origin = os.path.dirname(rel)
        dirs = []
        for d in elf.rpath + elf.runpath:
            d = d.replace('${ORIGIN}', '/' + origin).replace('$ORIGIN', '/' + origin)
            if d.startswith('/'):
                dirs.append(d)
        dirs += ['/' + d for d in self.ld]
        for d in dirs:
            cand = treescan.resolve(self.root, '', d.rstrip('/') + '/' + soname)
            if cand is None:
                continue
            real = treescan.lookup(self.root, cand)
            if real is None:
                continue
            try:
                if not stat.S_ISREG(os.stat(os.path.join(self.root, real)).st_mode):
                    continue
            except OSError:
                continue
            return treescan.canon(self.aliases, cand), real
        return None

    def lib_owner(self, cand, real):
        return self.owner.get(cand) or self.owner.get(treescan.canon(self.aliases, real))

    def soname_closure(self, rel):
        """Every soname rel maps, directly or through what it needs."""
        if rel in self.closures:
            return self.closures[rel]
        self.closures[rel] = set()
        out, todo, seen = set(), [rel], {rel}
        while todo:
            r = todo.pop()
            elf = self.parse(r)
            if not elf:
                continue
            for so in elf.needed:
                out.add(so)
                hit = self.find_lib(r, elf, so)
                if hit and hit[1] not in seen:
                    seen.add(hit[1])
                    todo.append(hit[1])
        self.closures[rel] = out
        return out


def shebang(path):
    try:
        with open(path, 'rb') as f:
            head = f.read(256)
    except OSError:
        return None
    if not head.startswith(b'#!'):
        return None
    line = head[2:].split(b'\n', 1)[0].decode('utf-8', 'replace').strip()
    return line.split() or None


def env_command(args):
    """The command `env` would run from a shebang's arguments."""
    i = 1
    while i < len(args):
        a = args[i]
        if a in ('-S', '--split-string'):
            i += 1
            continue
        if a.startswith('-S'):
            return a[2:].split()[0] if a[2:].strip() else None
        if a.startswith('-') or '=' in a:
            i += 1
            continue
        return a
    return None


def main():
    ap = argparse.ArgumentParser(description='run-time references a built tree cannot satisfy')
    ap.add_argument('--root', default='build/fs')
    ap.add_argument('--all', action='store_true', help='also list the unresolved warnings')
    a = ap.parse_args()

    repo_root = os.getcwd()
    if not os.path.isdir(os.path.join(repo_root, 'script', 'phases')):
        print('linkcheck: no script/phases here; run from the repository root', file=sys.stderr)
        return 2
    if not os.path.isdir(os.path.join(a.root, 'var/lib/kpkg/db')):
        print(f'linkcheck: no package database under {a.root}', file=sys.stderr)
        return 2
    t = Tree(a.root)
    root = a.root
    places, phase_repos, _, res = phase_places(repo_root)
    all_repos = [r for p in phase_repos for r in p]

    fails, warns = [], []
    count = {'undeclared': 0, 'interpreter': 0, 'openssl': 0,
             'python': 0, 'unresolved': 0, 'unowned': 0}

    def fail(kind, msg):
        count[kind] += 1
        fails.append(f'FAIL {kind:<11} {msg}')

    def warn(kind, msg):
        count[kind] += 1
        warns.append(f'WARN {kind:<11} {msg}')

    def interp_ok(path):
        rel = path.lstrip('/')
        return treescan.is_exec_file(root, rel)

    elves = []
    elf_pkg = {}
    for pkg in sorted(t.files):
        used = {}
        for rel in t.files[pkg]:
            full = os.path.join(root, rel)
            try:
                st = os.lstat(full)
            except OSError:
                continue
            if not stat.S_ISREG(st.st_mode):
                continue
            elf = t.parse(rel) if st.st_size >= 52 else None
            if elf:
                elves.append(rel)
                elf_pkg[rel] = pkg
                if elf.interp and not interp_ok(elf.interp):
                    fail('interpreter', f'{pkg}: {rel}: PT_INTERP {elf.interp} does not resolve')
                for so in elf.needed:
                    hit = t.find_lib(rel, elf, so)
                    if not hit:
                        warn('unresolved', f'{pkg}: {rel} needs {so}')
                        continue
                    o = t.lib_owner(*hit)
                    if not o:
                        warn('unowned', f'{pkg}: {rel} needs {so} = /{hit[1]}, owned by no package')
                    elif o != pkg:
                        used.setdefault(o, f'{rel} needs {so}')
                continue
            if not st.st_mode & 0o111:
                continue
            sb = shebang(full)
            if not sb:
                continue
            interp = sb[0]
            name = os.path.basename(interp)
            if name == 'env':
                cmd = env_command(sb)
                if cmd == 'python':
                    warn('python', f'{pkg}: {rel}: #!{" ".join(sb)}')
                elif cmd and '/' not in cmd and not treescan.which(root, cmd, PATH_DIRS):
                    fail('interpreter', f'{pkg}: {rel}: #!{" ".join(sb)}: no {cmd} on PATH')
                if not interp_ok(interp):
                    fail('interpreter', f'{pkg}: {rel}: #!{interp} does not resolve')
                continue
            if name == 'python':
                warn('python', f'{pkg}: {rel}: #!{interp}')
                continue
            if interp.startswith('/') and interp.lstrip('/').startswith(EXEC_ROOTS) \
                    and not interp_ok(interp):
                fail('interpreter', f'{pkg}: {rel}: #!{interp} does not resolve')

        for o in EXEMPT:
            used.pop(o, None)
        if not used:
            continue
        place = places.get(pkg)
        repos = phase_repos[place[0]] if place else all_repos
        declared = closure(res, repos, pkg)
        for o in sorted(used):
            if o not in declared:
                fail('undeclared', f'{pkg} -> {o} ({used[o]})')

    def crypto_both(sonames):
        return all(c in sonames for c in CRYPTO)

    def first(paths):
        for p in paths:
            if t.parse(p):
                return p
        return None

    qtnet = first(['usr/lib/libQt5Network.so.5'])
    pyext = [p for p in elves if p.startswith('usr/lib/python3') and '/lib-dynload/' in p
             and os.path.basename(p).startswith(('_hashlib.', '_ssl.'))]
    pybase = set()
    for p in pyext:
        pybase |= t.soname_closure(p)
    qtbase = t.soname_closure(qtnet) if qtnet else set()
    for rel in elves:
        own = t.soname_closure(rel)
        if crypto_both(own):
            fail('openssl', f'{rel} maps both through its own NEEDED closure')
        elif '/qt5/plugins/' in rel and qtbase and crypto_both(own | qtbase):
            fail('openssl', f'{rel} with libQt5Network.so.5')
        elif rel.startswith('usr/lib/python3') and rel.endswith('.so') and pybase \
                and crypto_both(own | pybase):
            if elf_pkg[rel] in PY_OPENSSL_ACCEPTED:
                warn('openssl', f'{rel} with _hashlib/_ssl (accepted: known-gaps.md)')
            else:
                fail('openssl', f'{rel} with _hashlib/_ssl')

    for line in fails:
        print(line)
    for line in warns:
        if a.all or not line.startswith('WARN unresolved'):
            print(line)
    print(f'linkcheck: {len(t.files)} packages, {len(elves)} ELF files; '
          + ', '.join(f'{k} {v}' for k, v in count.items()))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
