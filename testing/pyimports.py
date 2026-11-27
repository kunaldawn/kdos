#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   pyimports — Python imports a prebuilt install leaves unmet
# ---------------------------------
#
# Run from the repository root over a built tree:
#
#   python3 testing/pyimports.py [--root build/fs] [--all] [--pkg <name>]...
#                                [--strict]
#
# Every installed package's .py files, and its executable scripts whose #!
# names a Python, are parsed with ast. Each absolute import, at module level
# or inside a function, is read for its top-level name. An import is skipped
# when it is guarded: in the body or handler of a try that catches
# ImportError, ModuleNotFoundError, NameError, Exception or everything, in a
# `with suppress(ImportError)`, under `if TYPE_CHECKING`, or under an `if`
# that tests sys.platform, os.name, platform.system() or sys.version_info.
#
# A name resolves, in order, to the package itself, the tree's standard
# library (usr/lib/python3.N and its lib-dynload, plus the
# tree interpreter's built-in modules, read by running it through the tree's
# own loader), the importing file's own directory, and then the
# site-packages directories the
# file runs with: the system one, the private prefix's own one when the file
# sits under <prefix>/lib/python3.N/site-packages, and every directory a .pth
# file in either adds. Each module of the name found there names an owner, and
# one owner the package reaches is enough. The package's own names are the
# top-level modules and packages it ships in the standard library, a
# site-packages directory or a .pth directory, and those of a directory
# inside one that its program puts on sys.path itself (a vendor directory,
# one holding a .dist-info, one with no __init__.py). A file outside every
# such directory, under a private prefix a script adds, contributes its
# basename and every directory above it. Any other module or directory name
# in the package counts as its own only when no sys.path directory provides
# the name, so a subpackage named like a distribution
# (scikit_build_core.setuptools) does not hide an import of that distribution
# and a sibling import a script makes after adding its own directory does
# not read as missing. `distutils`, which Python 3.12 left
# out of the standard library, is setuptools' in a directory that holds it:
# setuptools' distutils-precedence.pth puts its copy in place.
#
# The owner is then placed against the importing package's recipe:
#
#   MISSING     nothing on the tree provides the name.
#   UNDECLARED  the owner is outside the package's `depends =` closure, so an
#               install that is not the build tree may not carry it.
#   CUT         the owner is in the closure only through a port of
#               ports/buildonly that no package on the path records as a
#               run-time need, so a prebuilt `kdos update install` drops it
#               (the binhost `N:` line, packaging.md "The binary host"). A
#               `runtime =` word on the importing recipe is the fix.
#
# A package's recorded needs are its db/.needs record resolved to owners, plus
# the `runtime =` words of its recipe as the tree now has it, so a recipe
# fixed since the build is measured as fixed.
#
# Files under a tests, examples, docs or benchmarks directory, and under
# usr/share/doc, are left out unless --all is given. Each finding prints the
# import's scope: `mod` runs when the file is imported, `cond` under some
# other `if`, `fn` only when a function runs. Exit status is 0 unless
# --strict is given and a CUT or UNDECLARED import at `mod` scope was found.
# It is advisory: preflight does not run it, because it needs a built tree.

import argparse
import ast
import os
import subprocess
import sys
import warnings

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'script', 'lib'))
from phaseclosure import read_depends  # noqa: E402
import imageset  # noqa: E402

SKIP_DIRS = {'tests', 'test', 'testing', 'examples', 'example', 'docs', 'doc',
             'benchmarks', 'benchmark', 'demos', 'demo', 'samples'}
VENDOR_DIRS = {'_vendor', 'vendor', '_vendored', 'vendored', 'extern', '_extern',
               'third_party', '_third_party', 'dist', 'lib'}
GUARD_EXC = {'ImportError', 'ModuleNotFoundError', 'NameError', 'Exception',
             'BaseException'}
LIB_DIRS = ('usr/lib', 'lib', 'usr/local/lib')


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
            if not ln:
                continue
            ln = ln[2:] if ln.startswith('./') else ln
            ln = ln.rstrip('/')
            if not ln:
                continue
            paths.append(ln)
            owner.setdefault(ln, name)
        files[name] = paths
    return files, owner


def mod_name(entry):
    """The importable name an entry of a module directory provides, or None."""
    if entry.endswith('.py'):
        n = entry[:-3]
    elif entry.endswith('.so'):
        n = entry.split('.', 1)[0]
    elif '.' in entry:
        return None
    else:
        n = entry
    return n if n.isidentifier() else None


class Dir:
    """One directory on sys.path: name -> owners, from the db and the disk."""

    def __init__(self, root, rel, owner, children):
        self.rel = rel
        self.names = {}
        for entry, paths in children.get(rel, {}).items():
            n = mod_name(entry)
            if n:
                for p in paths:
                    self.names.setdefault(n, set()).add(owner.get(p, '(unowned)'))
        if 'setuptools' in self.names and 'distutils' not in self.names:
            self.names['distutils'] = set(self.names['setuptools'])
        full = os.path.join(root, rel)
        try:
            for entry in os.listdir(full):
                n = mod_name(entry)
                if n and n not in self.names:
                    self.names[n] = {'(unowned)'}
        except OSError:
            pass


def site_of(rel):
    """The site-packages directory a path sits under, or None."""
    parts = rel.split('/')
    for i in range(len(parts) - 1, 0, -1):
        if parts[i] == 'site-packages':
            return '/'.join(parts[:i + 1])
    return None


def pth_dirs(root, site):
    out = []
    full = os.path.join(root, site)
    try:
        names = sorted(os.listdir(full))
    except OSError:
        return out
    for n in names:
        if not n.endswith('.pth'):
            continue
        try:
            with open(os.path.join(full, n), encoding='utf-8', errors='replace') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#') or line.startswith('import'):
                        continue
                    p = line if line.startswith('/') else os.path.join('/', site, line)
                    p = os.path.normpath(p).lstrip('/')
                    if os.path.isdir(os.path.join(root, p)):
                        out.append(p)
        except OSError:
            pass
    return out


def is_guard_handler(h):
    t = h.type
    if t is None:
        return True
    names = t.elts if isinstance(t, ast.Tuple) else [t]
    for e in names:
        if isinstance(e, ast.Name) and e.id in GUARD_EXC:
            return True
        if isinstance(e, ast.Attribute) and e.attr in GUARD_EXC:
            return True
    return False


def suppresses(e):
    """1 for contextlib.suppress() of an import error, which guards like a try."""
    if not isinstance(e, ast.Call):
        return False
    f = e.func
    name = f.attr if isinstance(f, ast.Attribute) else getattr(f, 'id', '')
    if name != 'suppress':
        return False
    for a in e.args:
        n = a.attr if isinstance(a, ast.Attribute) else getattr(a, 'id', '')
        if n in GUARD_EXC:
            return True
    return False


def guard_test(t):
    """1 for TYPE_CHECKING or a platform or version test, else 0."""
    for n in ast.walk(t):
        if isinstance(n, ast.Name) and n.id == 'TYPE_CHECKING':
            return True
        if isinstance(n, ast.Attribute):
            if n.attr == 'TYPE_CHECKING':
                return True
            if n.attr in ('platform', 'version_info', 'name'):
                if isinstance(n.value, ast.Name) and n.value.id in ('sys', 'os', 'typing'):
                    return True
            if n.attr == 'system' and isinstance(n.value, ast.Name) and n.value.id == 'platform':
                return True
    return False


def imports(tree):
    """[(top name, line, scope)] of every unguarded absolute import."""
    out = []

    def walk(nodes, scope):
        for n in nodes:
            if isinstance(n, ast.Import):
                for a in n.names:
                    out.append((a.name.split('.')[0], n.lineno, scope))
            elif isinstance(n, ast.ImportFrom):
                if n.level == 0 and n.module:
                    out.append((n.module.split('.')[0], n.lineno, scope))
            elif isinstance(n, (ast.Try, getattr(ast, 'TryStar', ast.Try))):
                if not any(is_guard_handler(h) for h in n.handlers):
                    walk(n.body, scope)
                    for h in n.handlers:
                        walk(h.body, scope)
                walk(n.orelse, scope)
                walk(n.finalbody, scope)
            elif isinstance(n, ast.If):
                if guard_test(n.test):
                    continue
                inner = scope if scope == 'fn' else 'cond'
                walk(n.body, inner)
                walk(n.orelse, inner)
            elif isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef, ast.Lambda)):
                walk(n.body if isinstance(n.body, list) else [], 'fn')
            elif isinstance(n, ast.ClassDef):
                walk(n.body, scope)
            elif isinstance(n, (ast.With, ast.AsyncWith)) and any(
                    suppresses(i.context_expr) for i in n.items):
                continue
            elif isinstance(n, (ast.With, ast.AsyncWith, ast.For, ast.AsyncFor, ast.While)):
                walk(n.body, scope)
                walk(getattr(n, 'orelse', []), scope)
            elif isinstance(n, ast.Match):
                for c in n.cases:
                    walk(c.body, 'cond' if scope != 'fn' else scope)

    walk(tree.body, 'mod')
    return out


def python_script(full):
    try:
        with open(full, 'rb') as f:
            head = f.readline(256)
    except OSError:
        return False
    return head.startswith(b'#!') and b'python' in head


def stdlib_names(root):
    libs = sorted(d for d in os.listdir(os.path.join(root, 'usr', 'lib'))
                  if d.startswith('python3.') and d[8:].isdigit())
    if not libs:
        raise SystemExit('pyimports: no usr/lib/python3.N in ' + root)
    lib = os.path.join('usr', 'lib', libs[-1])
    names = set()
    for d in (lib, os.path.join(lib, 'lib-dynload')):
        for e in os.listdir(os.path.join(root, d)):
            if e == 'site-packages':
                continue
            n = mod_name(e)
            if n:
                names.add(n)
    loader = os.path.join(root, 'usr', 'lib', 'libc.so')
    py = os.path.join(root, 'usr', 'bin', 'python3')
    builtin = None
    try:
        r = subprocess.run([loader, '--library-path', os.path.join(root, 'usr', 'lib'),
                            py, '-I', '-c',
                            'import sys; print(" ".join(sys.builtin_module_names))'],
                           capture_output=True, text=True, timeout=60)
        if r.returncode == 0:
            builtin = r.stdout.split()
    except (OSError, subprocess.SubprocessError):
        pass
    names.update(builtin if builtin is not None else sys.builtin_module_names)
    return lib, names


def main():
    ap = argparse.ArgumentParser(description='Python imports a prebuilt install leaves unmet')
    ap.add_argument('--root', default='build/fs')
    ap.add_argument('--all', action='store_true',
                    help='read tests, examples and documentation directories too')
    ap.add_argument('--pkg', action='append', default=[],
                    help='measure only this package (repeatable)')
    ap.add_argument('--strict', action='store_true',
                    help='exit 1 on a CUT or UNDECLARED import at module scope')
    a = ap.parse_args()

    repo = os.getcwd()
    if not os.path.isfile(os.path.join(repo, 'ports', 'buildonly')):
        print('pyimports: no ports/buildonly here; run from the repository root')
        return 1
    root = a.root
    files, owner = read_db(root)
    errors = []
    ports = imageset.recipes(repo)
    bo = imageset.buildonly(repo, ports, errors)
    lib, std = stdlib_names(root)

    children = {}
    for p in owner:
        d, _, e = p.rpartition('/')
        children.setdefault(d, {}).setdefault(e, []).append(p)
        dd, _, ee = d.rpartition('/')
        if dd:
            children.setdefault(dd, {}).setdefault(ee, []).append(p)
    dirs = {}

    def get_dir(rel):
        if rel not in dirs:
            dirs[rel] = Dir(root, rel, owner, children)
        return dirs[rel]

    system_site = lib + '/site-packages'
    path_cache = {}

    def search_path(rel):
        site = site_of(rel)
        key = site or ''
        if key not in path_cache:
            sites = [system_site]
            if site and site != system_site:
                sites.insert(0, site)
            out = []
            for s in sites:
                out.append(s)
                out += pth_dirs(root, s)
            path_cache[key] = [get_dir(d) for d in out]
        return path_cache[key]

    def runtime_words(name):
        d = ports.get(name)
        if not d:
            return []
        with open(os.path.join(d, 'kpkgbuild'), encoding='utf-8') as f:
            for line in f:
                k, sep, v = line.partition('=')
                if sep and k.strip() == 'runtime':
                    return v.split()
        return []

    all_site = [get_dir(s) for s in sorted({site_of(p) for p in owner if site_of(p)})]
    path_roots = {lib, lib + '/lib-dynload'}
    for d in all_site:
        path_roots.add(d.rel)
        path_roots.update(pth_dirs(root, d.rel))

    vendor_cache = {}

    def vendored(d):
        """True for a directory inside a package that its program puts on
        sys.path itself: one holding a .dist-info or .egg-info, one named
        like a vendor directory, or one with no __init__.py, which nothing
        could import as a subpackage."""
        if d not in vendor_cache:
            entries = children.get(d, {})
            vendor_cache[d] = (d.rsplit('/', 1)[-1] in VENDOR_DIRS
                               or '__init__.py' not in entries
                               or any(e.endswith('.dist-info') or e.endswith('.egg-info')
                                      for e in entries))
        return vendor_cache[d]

    def path_tops(rel):
        """The entry of rel below each sys.path directory it sits under, the
        ones a package puts there itself included, or None for a path under
        none of them."""
        ancestors = []
        d = rel
        while '/' in d:
            d = d.rsplit('/', 1)[0]
            ancestors.append(d)
        for i, d in enumerate(ancestors):
            if d in path_roots:
                tops = [rel[len(d) + 1:].split('/', 1)[0]]
                for v in ancestors[:i - 1] if i > 1 else []:
                    if vendored(v):
                        tops.append(rel[len(v) + 1:].split('/', 1)[0])
                return tops
        return None
    need_cache = {}

    def needed(name):
        """Owners of the package's recorded needs, plus its runtime = words."""
        if name in need_cache:
            return need_cache[name]
        out = set(runtime_words(name))
        rec = os.path.join(root, 'var', 'lib', 'kpkg', 'db', '.needs', name)
        try:
            with open(rec, encoding='utf-8') as f:
                lines = f.read().split('\n')
        except OSError:
            lines = None
        if lines is None:
            out.add('*')
        for ln in lines or []:
            kind, _, val = ln.partition(' ')
            if kind == 'need':
                out.add(val)
            elif kind == 'py':
                for d in all_site:
                    out |= d.names.get(val, set())
            elif kind == 'so':
                cands = [val.lstrip('/')] if val.startswith('/') else \
                    [d + '/' + val for d in LIB_DIRS]
                out |= {owner[c] for c in cands if c in owner}
            elif kind == 'exe':
                p = val.lstrip('/')
                if p in owner:
                    out.add(owner[p])
            elif kind == 'qml':
                for q in ('qt6', 'qt5'):
                    p = f'usr/lib/{q}/qml/{val.replace(".", "/")}/qmldir'
                    if p in owner:
                        out.add(owner[p])
        need_cache[name] = out
        return out

    def closures(name):
        full, pre = set(), set()
        stack = [name]
        while stack:
            n = stack.pop()
            if n in full or n not in ports:
                continue
            full.add(n)
            stack += read_depends(ports[n])
        stack = [name]
        while stack:
            n = stack.pop()
            if n in pre or n not in ports:
                continue
            pre.add(n)
            nd = None
            for dep in read_depends(ports[n]):
                if dep in bo:
                    if nd is None:
                        nd = needed(n)
                    if dep not in nd and '*' not in nd:
                        continue
                stack.append(dep)
        return full, pre

    wanted = set(a.pkg)
    found = []
    unparsed = 0
    read = 0
    for pkg in sorted(files):
        if wanted and pkg not in wanted:
            continue
        own = set()
        nested = set()
        srcs = []
        for rel in files[pkg]:
            base = rel.rsplit('/', 1)[-1]
            if base.endswith('.py') or base.endswith('.so'):
                if base.endswith('.py'):
                    srcs.append(rel)
                tops = path_tops(rel)
                for top in tops or []:
                    n = mod_name(top)
                    if n:
                        own.add(n)
                names = {base.split('.', 1)[0]}
                names.update(d for d in rel.split('/')[:-1] if d.isidentifier())
                if tops is None:
                    own |= names
                else:
                    nested |= names
            if (rel.startswith('usr/bin/') or rel.startswith('usr/sbin/')
                    or rel.startswith('usr/libexec/') or '/bin/' in rel) \
                    and not base.endswith('.py'):
                full = os.path.join(root, rel)
                if os.path.isfile(full) and not os.path.islink(full) \
                        and os.access(full, os.X_OK) and python_script(full):
                    srcs.append(rel)
        if not srcs:
            continue
        hits = {}
        for rel in srcs:
            if '__pycache__' in rel:
                continue
            parts = rel.split('/')
            if not a.all and (rel.startswith('usr/share/doc/')
                              or any(p in SKIP_DIRS for p in parts[:-1])
                              or parts[-1].startswith('test_')):
                continue
            full = os.path.join(root, rel)
            if os.path.islink(full) or not os.path.isfile(full):
                continue
            try:
                with open(full, 'rb') as f:
                    src = f.read()
                with warnings.catch_warnings():
                    warnings.simplefilter('ignore')
                    tree = ast.parse(src, filename=rel)
            except (SyntaxError, ValueError, OSError):
                unparsed += 1
                continue
            read += 1
            here = rel.rsplit('/', 1)[0]
            for name, line, scope in imports(tree):
                if name == '__future__' or name in own or name in std:
                    continue
                if name in get_dir(here).names:
                    continue
                owners = set()
                for d in search_path(rel):
                    owners = owners | d.names.get(name, set())
                if not owners and name in nested:
                    continue
                hits.setdefault((name, frozenset(owners)), []).append((scope, rel, line))
        if not hits:
            continue
        full_c, pre_c = closures(pkg)
        for (name, owners), where in sorted(hits.items(), key=lambda kv: kv[0][0]):
            if not owners:
                kind, who = 'MISSING', '-'
            elif owners & pre_c or '(unowned)' in owners or pkg in owners:
                continue
            elif owners & full_c:
                kind, who = 'CUT', ' '.join(sorted(owners & full_c))
            else:
                kind, who = 'UNDECLARED', ' '.join(sorted(owners))
            rank = {'mod': 0, 'cond': 1, 'fn': 2}
            where.sort(key=lambda w: (rank[w[0]], w[1], w[2]))
            scope, rel, line = where[0]
            more = len({w[1] for w in where}) - 1
            found.append((kind, pkg, name, who, scope, f'{rel}:{line}', more))

    order = {'CUT': 0, 'UNDECLARED': 1, 'MISSING': 2}
    for kind, pkg, name, who, scope, at, more in sorted(found, key=lambda f: (order[f[0]], f[1], f[2])):
        tail = f' (+{more} file{"s" if more > 1 else ""})' if more else ''
        print(f'{kind:<10} {pkg} imports {name} -> {who} [{scope}] {at}{tail}')
    counts = {k: sum(1 for f in found if f[0] == k) for k in order}
    print(f'pyimports: {read} files read, {unparsed} not parsed; '
          + ', '.join(f'{k} {v}' for k, v in counts.items()))
    bad = [f for f in found if f[0] in ('CUT', 'UNDECLARED') and f[4] == 'mod']
    return 1 if a.strict and bad else 0


if __name__ == '__main__':
    sys.exit(main())
