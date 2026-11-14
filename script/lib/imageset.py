#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   script/lib/imageset.py — which packages the installation medium carries
# ---------------------------------
#
#   imageset.py [--tree DIR] [--root DIR] (--keep | --drop | --check | --sizes)
#   imageset.py [--tree DIR] --why PACKAGE
#   imageset.py [--tree DIR] [--root DIR] --ledger [--out FILE]
#   imageset.py [--tree DIR] [--root DIR] --verify [--full DIR] [--needs DIR]
#               [--all]
#
#   --tree    the repository: ports/, src/ and script/phases/ under it
#             (default: two levels above this file)
#   --root    the root whose package database is read (default /)
#   --keep    print every installed package the image holds, one per line
#   --drop    print every installed package it does not
#   --check   read ports/base, ports/buildonly and ports/groups against the
#             tree and exit 1 on a name that is not a port, a phase or area
#             that does not exist, or a group id that is a port's name; needs
#             no package database
#   --sizes   the kept and dropped package counts and the bytes their files
#             take in the root
#   --why     the shortest dependency path from a ports/base seed to the
#             package, `seed > dep > ... > package`; exit 1 and "not in the
#             base" when no path reaches it. Needs no package database
#   --ledger  every installed package in the root, largest first: its bytes,
#             its MiB, its name and its --why path ("-" outside the base),
#             then a `total` line; to FILE with --out, else to stdout
##   --verify  the base against the run-time needs its packages recorded
#             (kpkg needs, `<root>/var/lib/kpkg/db/.needs/<name>`): exit 1
#             when a kept package has an edge that only packages outside the
#             base provide. --full names the tree the root was trimmed from
#             (the stage's build/fs), whose packages are the ones the root
#             no longer holds; --needs reads the records from DIR instead
#             (a `kpkg needs --scan` of each package). An edge nothing
#             provides, and a kept package with no record, are counted, and
#             listed with --all; neither fails
#
# THE SET is ports/base's seeds and their closure over each recipe's first
# `depends =` line (the line the solver reads), a dependency on a port
# ports/buildonly lists not followed unless a seed names it. A seed or a
# dependency with no recipe is skipped: a package installed with no recipe
# behind it is in no set, and --drop names it. --why walks the same edges
# breadth first from every seed at once, so its path is one the closure took
# and a package --keep prints always has one.
#
# A PACKAGE NAMED `<port>:<facet>` has no recipe of its own and is in no set:
# the image takes the run-time package, which carries the port's own name,
# and --drop names the others.
#
# BYTES COUNT EACH INODE ONCE. A file is lstat()ed where its package's
# manifest names it; a hard link two packages share is counted for the first
# of them in name order, so the ledger's total is the bytes the root's
# package files take, which is what the image compresses.
#
# THE PROOF OF THE BASE is --verify. The selection follows `depends =`,
# which cannot see a QML import, a Python import or a plugin loaded by name;
# the records can, and an edge resolves the way libkpkg's kp_needs_where
# resolves it (kpkg.h): a soname in the loader's directories, an interpreter
# at its path, a QML module's qmldir under usr/lib/qt6/qml or qt5/qml (a
# QtQml, QtQuick or QML module with none is qt6-qtdeclarative's), a Python
# name under a site-packages directory, a `need` word as the package of
# that name. An edge is met when the package itself, a kept package or a
# file no package owns provides it. A change to either resolver changes the
# gate.
#
# Recipes are found the way kpkg finds them: in a repository, directly or one
# shelf down. The repositories are ports/core and src/system, src/art,
# src/desktop and src/daemons, the ones 70_image/040_orphans.sh searches.

import argparse
import os
import sys

AREAS = ("system", "art", "desktop", "daemons")


def words(path):
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                out.append(line.split())
    return out


def recipes(tree):
    found = {}
    repos = [os.path.join(tree, "ports", "core")]
    repos += [os.path.join(tree, "src", a) for a in AREAS]
    for repo in repos:
        if not os.path.isdir(repo):
            continue
        for d in sorted(os.listdir(repo)):
            p = os.path.join(repo, d)
            if os.path.isfile(os.path.join(p, "kpkgbuild")):
                found.setdefault(d, p)
                continue
            if not os.path.isdir(p):
                continue
            for n in sorted(os.listdir(p)):
                q = os.path.join(p, n)
                if os.path.isfile(os.path.join(q, "kpkgbuild")):
                    found.setdefault(n, q)
    return found


def recipe_depends(d):
    with open(os.path.join(d, "kpkgbuild"), encoding="utf-8") as f:
        for line in f:
            k, sep, v = line.partition("=")
            if sep and k.strip() == "depends":
                return v.split()
    return []


def phase_names(tree, phase):
    d = os.path.join(tree, "script", "phases", phase)
    one = os.path.join(d, "packages.txt")
    files = [one] if os.path.isfile(one) else []
    pd = os.path.join(d, "packages.d")
    if not files and os.path.isdir(pd):
        files = [os.path.join(pd, f) for f in sorted(os.listdir(pd))
                 if f.endswith(".txt")]
    if not files:
        return None
    return [n for f in files for line in words(f) for n in line]


def seeds(tree, ports, errors):
    out = []
    for line in words(os.path.join(tree, "ports", "base")):
        if line[0] == "phase":
            for ph in line[1:]:
                names = phase_names(tree, ph)
                if names is None:
                    errors.append(f"ports/base: no phase list {ph}")
                else:
                    out += names
        elif line[0] == "src":
            for a in line[1:]:
                d = os.path.join(tree, "src", a)
                if a not in AREAS or not os.path.isdir(d):
                    errors.append(f"ports/base: no src area {a}")
                    continue
                out += [n for n in sorted(os.listdir(d))
                        if os.path.isfile(os.path.join(d, n, "kpkgbuild"))]
        else:
            for n in line:
                if n not in ports:
                    errors.append(f"ports/base: {n} is not a port")
                out.append(n)
    return out


def buildonly(tree, ports, errors):
    out = set()
    for line in words(os.path.join(tree, "ports", "buildonly")):
        for n in line:
            if n not in ports:
                errors.append(f"ports/buildonly: {n} is not a port")
            out.add(n)
    return out


def groups(tree, ports, errors):
    out = {}
    cur = None
    path = os.path.join(tree, "ports", "groups")
    if not os.path.isfile(path):
        return out
    for line in words(path):
        if line[0] == "group":
            if len(line) < 2:
                errors.append("ports/groups: a group line with no id")
                cur = None
                continue
            cur = line[1]
            if cur in ports:
                errors.append(f"ports/groups: {cur} is a port's name, not a group id")
            if cur in out:
                errors.append(f"ports/groups: {cur} is named twice")
            out[cur] = []
        elif cur is None:
            errors.append(f"ports/groups: {line[0]} comes before any group line")
        else:
            for n in line:
                if n not in ports:
                    errors.append(f"ports/groups: {n} in {cur} is not a port")
                out[cur].append(n)
    for g, members in out.items():
        if not members:
            errors.append(f"ports/groups: {g} has no member")
    return out


def closure(ports, seed, skip):
    named = set(seed)
    seen = set()
    stack = list(seed)
    while stack:
        n = stack.pop()
        if n in seen or n not in ports:
            continue
        seen.add(n)
        for d in recipe_depends(ports[n]):
            if d in skip and d not in named:
                continue
            stack.append(d)
    return seen


def installed(root):
    db = os.path.join(root, "var", "lib", "kpkg", "db")
    return sorted(n for n in os.listdir(db)
                  if not n.startswith(".") and os.path.isfile(os.path.join(db, n)))


def files_size(root, name, seen=None):
    total = 0
    if seen is None:
        seen = set()
    with open(os.path.join(root, "var", "lib", "kpkg", "db", name),
              encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line.startswith("./") or line.endswith("/"):
                continue
            try:
                st = os.lstat(os.path.join(root, line[2:]))
            except OSError:
                continue
            if (st.st_dev, st.st_ino) in seen:
                continue
            seen.add((st.st_dev, st.st_ino))
            total += st.st_size
    return total


def paths(ports, seed, skip):
    named = set(seed)
    parent = {}
    queue = []
    for n in seed:
        if n in ports and n not in parent:
            parent[n] = None
            queue.append(n)
    i = 0
    while i < len(queue):
        n = queue[i]
        i += 1
        for d in recipe_depends(ports[n]):
            if d in parent or d not in ports:
                continue
            if d in skip and d not in named:
                continue
            parent[d] = n
            queue.append(d)
    return parent


def why(parent, name):
    if name not in parent:
        return None
    chain = []
    n = name
    while n is not None:
        chain.append(n)
        n = parent[n]
    return " > ".join(reversed(chain))


def ledger(root, parent):
    seen = set()
    rows = [(files_size(root, n, seen), n) for n in installed(root)]
    rows.sort(key=lambda r: (-r[0], r[1]))
    out = ["# bytes          MiB  package                         why"]
    for size, n in rows:
        out.append(f"{size:>12} {size / 1048576:>9.1f}  {n:<30}  {why(parent, n) or '-'}")
    total = sum(r[0] for r in rows)
    out.append(f"total {total} bytes, {total / 1048576:.1f} MiB, in {len(rows)} packages")
    return "\n".join(out) + "\n"


ALIASES = ("bin", "sbin", "lib", "lib64", "lib32")
QML_DIRS = ("usr/lib/qt6/qml", "usr/lib/qt5/qml")
QML_BUILTIN = ("QtQml", "QtQuick", "QML")
QML_BUILTIN_PKG = "qt6-qtdeclarative"


def canon_load(root):
    out = {}
    for top in ALIASES:
        try:
            if os.readlink(os.path.join(root, top)).rstrip("/") == "usr/" + top:
                out[top] = "usr/" + top
        except OSError:
            pass
    return out


def canon(aliases, rel):
    rel = rel.lstrip("/")
    head, sep, rest = rel.partition("/")
    if head in aliases:
        return aliases[head] + sep + rest
    return rel


def ld_dirs(root):
    try:
        with open(os.path.join(root, "etc", "ld-musl-x86_64.path"), encoding="utf-8") as f:
            dirs = [d.strip().strip("/") for d in f.read().replace(":", "\n").split("\n")]
        dirs = [d for d in dirs if d]
        if dirs:
            return dirs
    except OSError:
        pass
    return ["lib", "usr/local/lib", "usr/lib"]


def site_dirs(root):
    lib = os.path.join(root, "usr", "lib")
    try:
        names = sorted(os.listdir(lib))
    except OSError:
        return []
    return [f"usr/lib/{n}/site-packages" for n in names
            if n.startswith("python3.")
            and os.path.isdir(os.path.join(lib, n, "site-packages"))]


def real_rel(root, rel, cache):
    """rel with its directory resolved through the root's symlinks
    (usr/lib64 -> lib), so a file has one spelling; rel when the directory
    leaves the root."""
    d, _, b = rel.rpartition("/")
    key = ("real", root, d)
    if key not in cache:
        top = os.path.realpath(root)
        r = os.path.realpath(os.path.join(root, d))
        cache[key] = os.path.relpath(r, top) if (r + "/").startswith(top + "/") else d
    rd = cache[key]
    return b if rd in ("", ".") else f"{rd}/{b}"


def present(root, rel):
    return os.path.lexists(os.path.join(root, rel))


def where(root, edge, cache):
    """(paths under root that provide edge, the package it names or None)."""
    key = (root, edge)
    if key in cache:
        return cache[key]
    kind, _, v = edge.partition(" ")
    paths, byname = [], None
    if kind == "so":
        if "/" in v:
            if present(root, v.lstrip("/")):
                paths.append(v.lstrip("/"))
        else:
            for d in cache.setdefault(("ld", root), ld_dirs(root)):
                if present(root, f"{d}/{v}"):
                    paths.append(f"{d}/{v}")
    elif kind == "exe":
        r = v.lstrip("/")
        if r and present(root, r):
            paths.append(r)
    elif kind == "qml":
        p = v.replace(".", "/")
        for d in QML_DIRS:
            if present(root, f"{d}/{p}/qmldir"):
                paths.append(f"{d}/{p}/qmldir")
        if not paths and v.split(".", 1)[0] in QML_BUILTIN:
            byname = QML_BUILTIN_PKG
    elif kind == "py":
        for sp in cache.setdefault(("sp", root), site_dirs(root)):
            for c in (f"{sp}/{v}/__init__.py", f"{sp}/{v}.py"):
                if present(root, c):
                    paths.append(c)
            listing = cache.get(("ls", root, sp))
            if listing is None:
                listing = sorted(os.listdir(os.path.join(root, sp)))
                cache[("ls", root, sp)] = listing
            paths += [f"{sp}/{n}" for n in listing
                      if n.startswith(v + ".") and n.endswith(".so")]
            d = os.path.join(root, sp, v)
            if os.path.isdir(d) and not present(root, f"{sp}/{v}/__init__.py"):
                for n in sorted(os.listdir(d)):
                    if not os.path.isdir(os.path.join(d, n)) or os.path.islink(os.path.join(d, n)):
                        paths.append(f"{sp}/{v}/{n}")
    elif kind == "need":
        byname = v
    cache[key] = (paths, byname)
    return paths, byname


def owners(root, wanted):
    """{canonical path: sorted owners} for the paths of `wanted` the
    manifests in root claim."""
    aliases = canon_load(root)
    out = {}
    db = os.path.join(root, "var", "lib", "kpkg", "db")
    for n in installed(root):
        with open(os.path.join(db, n), encoding="utf-8", errors="surrogateescape") as f:
            next(f, None)
            for line in f:
                line = line.rstrip("\n")
                if not line.startswith("./") or line.endswith("/"):
                    continue
                k = canon(aliases, line[2:])
                if k in wanted:
                    out.setdefault(k, set()).add(n)
    return out


def read_needs(d, name):
    try:
        with open(os.path.join(d, name), encoding="utf-8", errors="surrogateescape") as f:
            return [l.rstrip("\n") for l in f if l.strip()]
    except OSError:
        return None


def verify(root, full, needs_dir, keep, show_all):
    have = installed(root)
    have_set = set(have)
    full_set = set(installed(full)) if full else set()
    kept = [n for n in have if n in keep]
    needs_dir = needs_dir or os.path.join(root, "var", "lib", "kpkg", "db", ".needs")
    cache = {}
    edges, unmeasured = {}, []
    for n in kept:
        rec = read_needs(needs_dir, n)
        if rec is None:
            unmeasured.append(n)
        else:
            edges[n] = rec
    trees = [root] + ([full] if full else [])
    wanted = {t: set() for t in trees}
    for rec in edges.values():
        for e in rec:
            for t in trees:
                a = canon_load(t)
                for p in where(t, e, cache)[0]:
                    wanted[t].add(canon(a, p))
                    wanted[t].add(canon(a, real_rel(t, p, cache)))
    own = {t: owners(t, wanted[t]) for t in trees}
    alias = {t: canon_load(t) for t in trees}

    def providers(t, e, installed_names):
        paths, byname = where(t, e, cache)
        who, loose = set(), False
        for p in paths:
            o = own[t].get(canon(alias[t], p)) or \
                own[t].get(canon(alias[t], real_rel(t, p, cache)))
            if o:
                who |= o
            else:
                loose = True
        if byname and byname in installed_names:
            who.add(byname)
        return who, loose

    fails, nothing = [], []
    nedges = 0
    for n in sorted(edges):
        for e in edges[n]:
            nedges += 1
            who, loose = providers(root, e, have_set)
            if n in who or who & keep or loose:
                continue
            if who:
                fails.append(f"FAIL {n}: {e}: only {', '.join(sorted(who))} "
                             f"provide{'s' if len(who) == 1 else ''} it, outside the base")
                continue
            if full:
                fwho, _ = providers(full, e, full_set)
                if fwho:
                    fails.append(f"FAIL {n}: {e}: only {', '.join(sorted(fwho))} "
                                 f"provide{'s' if len(fwho) == 1 else ''} it, and the image drops "
                                 f"{'it' if len(fwho) == 1 else 'them'}")
                    continue
            nothing.append(f"{n}: {e}")
    for f in fails:
        print(f)
    if show_all:
        for x in nothing:
            print(f"nothing provides {x}")
        for x in unmeasured:
            print(f"unmeasured {x}")
    print(f"imageset: verify: {len(edges)} kept package(s) measured, "
          f"{len(unmeasured)} unmeasured, {nedges} edge(s), "
          f"{len(fails)} met only outside the base, {len(nothing)} met by nothing")
    return 1 if fails else 0


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description="the installation medium's packages")
    ap.add_argument("--tree", default=os.path.dirname(os.path.dirname(here)))
    ap.add_argument("--root", default="/")
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--keep", action="store_true")
    mode.add_argument("--drop", action="store_true")
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--sizes", action="store_true")
    mode.add_argument("--why", metavar="PACKAGE")
    mode.add_argument("--ledger", action="store_true")
    mode.add_argument("--verify", action="store_true")
    ap.add_argument("--out", metavar="FILE")
    ap.add_argument("--full", metavar="DIR")
    ap.add_argument("--needs", metavar="DIR")
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    if a.out and not a.ledger:
        ap.error("--out goes with --ledger")
    if (a.full or a.needs or a.all) and not a.verify:
        ap.error("--full, --needs and --all go with --verify")

    errors = []
    ports = recipes(a.tree)
    seed = seeds(a.tree, ports, errors)
    skip = buildonly(a.tree, ports, errors)
    grp = groups(a.tree, ports, errors)
    if errors:
        for e in errors:
            print(f"imageset: {e}", file=sys.stderr)
        return 1
    if a.check:
        print(f"imageset: ok ({len(set(seed))} seeds, {len(skip)} build-only, "
              f"{len(closure(ports, seed, skip))} ports in the closure, "
              f"{len(grp)} groups)")
        return 0

    if a.why:
        w = why(paths(ports, seed, skip), a.why)
        print(w if w else f"{a.why}: not in the base")
        return 0 if w else 1
    if a.ledger:
        text = ledger(a.root, paths(ports, seed, skip))
        if a.out:
            tmp = a.out + ".new"
            with open(tmp, "w", encoding="utf-8") as f:
                f.write(text)
            os.replace(tmp, a.out)
        else:
            sys.stdout.write(text)
        return 0

    keep = closure(ports, seed, skip)
    if a.verify:
        return verify(a.root, a.full, a.needs, keep, a.all)
    have = installed(a.root)
    if a.keep:
        print("\n".join(n for n in have if n in keep))
    elif a.drop:
        print("\n".join(n for n in have if n not in keep))
    else:
        k = [n for n in have if n in keep]
        d = [n for n in have if n not in keep]
        ks = sum(files_size(a.root, n) for n in k)
        ds = sum(files_size(a.root, n) for n in d)
        print(f"kept {len(k)} packages, {ks / 1e9:.2f} GB; "
              f"dropped {len(d)}, {ds / 1e9:.2f} GB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
