#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   defaultscheck — every KDOS default reads back as its fixture table says
# ---------------------------------
#
#   python3 testing/defaultscheck.py [--root build/fs] [--table FILE]
#   python3 testing/defaultscheck.py --kreadconfig CMD [--root build/fs]
#   python3 testing/defaultscheck.py --selftest
#
# Run from the repository root. testing/fixtures/defaultscheck/defaults.tsv
# holds one row a key (its header says how to read it). Each is read the way
# a KDE program reads it: through the XDG config cascade
# /etc/xdg:/usr/share/kdos/xdg, the earlier directory winning a key, with the
# overlay /usr/share/kdos/xdg-vm in front for a `vm` row and
# /usr/share/kdos/xdg-live in front for a `live` row.
#
# THE DEFAULT RUN reads fs/ and the few defaults a port of ours installs
# (PORT_FILES, from the port's source), or a built tree's etc/xdg and
# usr/share/kdos with --root, through this file's own KConfig reader: groups
# nested as
# [a][b], `#` comments, the value escapes, [$e] expansion of $HOME, and [$i]
# immutability, under which no higher file can change a key or a group. It
# needs nothing but python3, so it runs in `make check` on a bare host.
#
# --kreadconfig CMD reads every row with the real kreadconfig6 instead — CMD
# is split on spaces and run with --file/--group/--key appended, under a
# scratch HOME and XDG_CONFIG_DIRS — which is the proof that the reader here
# agrees with KConfig. For an `immutable` row it first writes the opposite
# value into the scratch ~/.config and requires the shipped one to survive.
#
# --selftest runs the reader against a tree that breaks each kind of row and
# requires each to fail: the proof the check can fail.
#
# Exit 0 every row holds, 1 a row does not, 2 a fixture or tree that does not
# parse.

import argparse
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

TABLE = "testing/fixtures/defaultscheck/defaults.tsv"
KINDS = ("kdos", "immutable", "kde", "vm", "live")

# Shipped defaults a port of ours installs rather than fs/, by the path they
# land at. With no built tree each is read from its port's source; a port
# that edits its copy at build time (kdos-plasma-look merges the accent's
# colours into kdeglobals) changes no key the table holds.
PORT_FILES = {
    "usr/share/kdos/xdg/kdeglobals": "src/desktop/kdos-plasma-look/xdg/kdeglobals",
}

# A staged file's path -> the repository path it was copied from, so a
# failure names the file to edit.
ORIGIN = {}


# ── KConfig, as far as the shipped files use it ──────────────────────────

ESCAPES = {"\\": "\\", "n": "\n", "t": "\t", "r": "\r", "s": " ", ";": ";", ",": ","}


def unescape(v):
    out, i = [], 0
    while i < len(v):
        c = v[i]
        if c == "\\" and i + 1 < len(v):
            n = v[i + 1]
            if n in ESCAPES:
                out.append(ESCAPES[n])
                i += 2
                continue
            if n == "x" and i + 3 < len(v):
                try:
                    out.append(chr(int(v[i + 2:i + 4], 16)))
                    i += 4
                    continue
                except ValueError:
                    pass
        out.append(c)
        i += 1
    return "".join(out)


def split_header(s):
    """'[a][b][$i]' -> (('a', 'b'), immutable). None for a line that is not
    a header."""
    if not (s.startswith("[") and s.endswith("]")):
        return None
    parts, cur, depth = [], [], 0
    for c in s:
        if c == "[" and depth == 0:
            depth, cur = 1, []
        elif c == "]" and depth == 1:
            depth = 0
            parts.append("".join(cur))
        elif depth == 1:
            cur.append(c)
    immutable = bool(parts) and parts[-1] == "$i"
    if immutable:
        parts = parts[:-1]
    return tuple(parts), immutable


def parse_kconfig(path):
    """[(group tuple, key, value, flags)] and the groups marked [$i]."""
    entries, frozen, group, errors = [], set(), None, []
    with open(path, encoding="utf-8", errors="replace") as f:
        for n, raw in enumerate(f, 1):
            s = raw.strip()
            if not s or s.startswith("#"):
                continue
            h = split_header(s)
            if h is not None:
                group, imm = h
                if imm:
                    frozen.add(group)
                continue
            if "=" not in s:
                errors.append("%s:%d: not a key=value line" % (path, n))
                continue
            if group is None:
                group = ()
            k, v = s.split("=", 1)
            k = k.strip()
            flags = ""
            if k.endswith("]") and "[$" in k:
                k, flags = k[:k.index("[$")], k[k.index("[$") + 2:-1]
            elif k.endswith("]") and "[" in k:
                continue  # a translated entry; KDOS ships English only
            entries.append((group, k.strip(), v.strip(), flags))
    return entries, frozen, errors


class Cascade:
    """One file name read along config dirs, highest priority first, the way
    KConfig merges them: lowest first, a [$i] entry or group blocking every
    file above it."""

    def __init__(self, dirs, name, home):
        self.values, self.locked, self.errors, self.home = {}, set(), [], home
        locked_groups = set()
        for d in reversed(dirs):
            p = os.path.join(d, name)
            if not os.path.isfile(p):
                continue
            entries, frozen, errs = parse_kconfig(p)
            self.errors += errs
            for group, key, val, flags in entries:
                gk = (group, key)
                if gk in self.locked or any(group[:i] in locked_groups for i in range(1, len(group) + 1)):
                    continue
                self.values[gk] = (val, "e" in flags, p)
                if "i" in flags:
                    self.locked.add(gk)
            locked_groups |= frozen
            for group in frozen:
                for gk in list(self.values):
                    if gk[0] == group:
                        self.locked.add(gk)

    def read(self, group, key):
        hit = self.values.get((group, key))
        if hit is None:
            return None, None
        val, expand, src = hit
        src = ORIGIN.get(os.path.abspath(src), src)
        val = unescape(val)
        if expand:
            val = val.replace("$HOME", self.home)
        return val, src

    def immutable(self, group, key):
        return (group, key) in self.locked


# ── the table ────────────────────────────────────────────────────────────

def load_table(path):
    rows, errors = [], []
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            line = raw.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            cols = line.split("\t")
            if len(cols) != 6:
                errors.append("%s:%d: %d columns, not 6" % (path, n, len(cols)))
                continue
            name, group, key, val, kind, why = cols
            if kind not in KINDS:
                errors.append("%s:%d: kind %r is not one of %s" % (path, n, kind, ", ".join(KINDS)))
                continue
            rows.append({"n": n, "file": name, "group": tuple(group.split("][")),
                         "key": key, "value": val, "kind": kind, "why": why})
    return rows, errors


def stage_tree(scratch):
    """The shipped config dirs as an image lays them out, built in scratch
    from fs/ and PORT_FILES. A path both fs/ and a port ship has two owners:
    the package overwrites the fs/ copy at every install."""
    errors = []
    for top in ("etc/xdg", "usr/share/kdos"):
        if os.path.isdir(os.path.join("fs", top)):
            shutil.copytree(os.path.join("fs", top), os.path.join(scratch, top), symlinks=True)
    for dirpath, _, files in os.walk(scratch):
        for f in files:
            p = os.path.join(dirpath, f)
            ORIGIN[os.path.abspath(p)] = os.path.join("fs", os.path.relpath(p, scratch))
    for dest, src in sorted(PORT_FILES.items()):
        if os.path.lexists(os.path.join("fs", dest)):
            errors.append("fs/%s is also installed by a port, from %s" % (dest, src))
        if not os.path.isfile(src):
            errors.append("%s, which installs /%s, is missing" % (src, dest))
            continue
        out = os.path.join(scratch, dest)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        shutil.copyfile(src, out)
        ORIGIN[os.path.abspath(out)] = src
    return errors


def tree_dirs(root):
    base = root if root else "fs"
    xdg = [os.path.join(base, "etc/xdg"), os.path.join(base, "usr/share/kdos/xdg")]
    overlays = {k: os.path.join(base, "usr/share/kdos/xdg-" + k) for k in ("vm", "live")}
    return xdg, overlays


def row_dirs(r, xdg, overlays):
    return ([overlays[r["kind"]]] + xdg) if r["kind"] in overlays else xdg


def check_native(rows, root, home="/home/kdos"):
    xdg, overlays = tree_dirs(root)
    caches, failures, errors = {}, [], []

    def cascade(name, dirs):
        k = (name, tuple(dirs))
        if k not in caches:
            caches[k] = Cascade(dirs, name, home)
            errors.extend(caches[k].errors)
        return caches[k]

    for r in rows:
        c = cascade(r["file"], row_dirs(r, xdg, overlays))
        got, src = c.read(r["group"], r["key"])
        where = "%s [%s] %s" % (r["file"], "][".join(r["group"]), r["key"])
        if r["kind"] == "kde":
            if got is not None:
                failures.append("%s is set to %r by %s; the table keeps KDE's %r"
                                % (where, got, src, r["value"]))
            continue
        if got is None:
            failures.append("%s is set by no shipped file; the table says %r" % (where, r["value"]))
        elif got != r["value"]:
            failures.append("%s is %r (%s); the table says %r" % (where, got, src, r["value"]))
        elif r["kind"] == "immutable" and not c.immutable(r["group"], r["key"]):
            failures.append("%s is %r but not immutable: a person's own file would change it"
                            % (where, got))
    return failures, errors


def check_kreadconfig(rows, root, cmd):
    xdg, overlays = tree_dirs(root)
    failures = []
    scratch = tempfile.mkdtemp(prefix="defaultscheck.")
    try:
        cfg = os.path.join(scratch, ".config")
        for r in rows:
            os.makedirs(cfg, exist_ok=True)
            dirs = row_dirs(r, xdg, overlays)
            env = {"HOME": scratch, "XDG_CONFIG_HOME": cfg, "PATH": os.environ.get("PATH", ""),
                   "XDG_CONFIG_DIRS": ":".join(os.path.abspath(d) for d in dirs)}
            if r["kind"] == "immutable":
                opposite = "true" if r["value"] == "false" else "false"
                with open(os.path.join(cfg, r["file"]), "w", encoding="utf-8") as f:
                    f.write("[%s]\n%s=%s\n" % ("][".join(r["group"]), r["key"], opposite))
            argv = shlex.split(cmd) + ["--file", r["file"]]
            for g in r["group"]:
                argv += ["--group", g]
            argv += ["--key", r["key"]]
            if r["kind"] == "kde":
                argv += ["--default", "\x01unset"]
            p = subprocess.run(argv, env=env, capture_output=True, text=True)
            got = p.stdout.rstrip("\n")
            where = "%s [%s] %s" % (r["file"], "][".join(r["group"]), r["key"])
            if p.returncode != 0:
                failures.append("%s: %s failed: %s" % (where, argv[0], p.stderr.strip()))
            elif r["kind"] == "kde":
                if got != "\x01unset":
                    failures.append("%s reads %r through KConfig; the table keeps KDE's %r"
                                    % (where, got, r["value"]))
            elif got != r["value"]:
                failures.append("%s reads %r through KConfig; the table says %r" % (where, got, r["value"]))
            shutil.rmtree(cfg, ignore_errors=True)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    return failures


# ── the proof it can fail ────────────────────────────────────────────────

def selftest():
    t = tempfile.mkdtemp(prefix="defaultscheck-self.")
    ok = True
    try:
        def put(rel, text):
            p = os.path.join(t, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "w", encoding="utf-8") as f:
                f.write(text)
        put("usr/share/kdos/xdg/kdeglobals",
            "# a comment = with an equals sign\n[KDE]\nAnimationDurationFactor=0.5\n"
            "[KDE Action Restrictions][$i]\nghns=false\n[Group][Sub]\nPath[$e]=$HOME/x\n")
        put("etc/xdg/kdeglobals", "[KDE]\nAnimationDurationFactor=1\n[KDE Action Restrictions]\nghns=true\n")
        put("usr/share/kdos/xdg/kwinrc", "[Windows]\nPlacement=Smart\n")
        put("usr/share/kdos/xdg-vm/kscreenlockerrc", "[Daemon]\nAutolock=false\n")
        put("usr/share/kdos/xdg-live/baloofilerc", "[Basic Settings]\nIndexing-Enabled=false\n")
        put("usr/share/kdos/xdg/baloofilerc", "[Basic Settings]\nIndexing-Enabled=true\n")
        put("usr/share/kdos/xdg/kaccessrc", "[ScreenReader]\nSystemdManaged=false\n")

        def run(rows):
            return check_native(rows, t, home="/h")[0]

        def row(**kw):
            r = {"n": 0, "file": "kdeglobals", "group": ("KDE",), "key": "AnimationDurationFactor",
                 "value": "0.5", "kind": "kdos", "why": ""}
            r.update(kw)
            return r

        cases = [
            ("/etc/xdg outranks /usr/share/kdos/xdg", [row(value="1")], False),
            ("a value the table does not hold fails", [row(value="0.5")], True),
            ("[$i] in the lower dir blocks /etc/xdg", [row(group=("KDE Action Restrictions",), key="ghns",
                                                          value="false", kind="immutable")], False),
            ("an immutable row on a mutable key fails", [row(file="kaccessrc", group=("ScreenReader",),
                                                             key="SystemdManaged", value="false",
                                                             kind="immutable")], True),
            ("[$e] expands $HOME, groups nest", [row(group=("Group", "Sub"), key="Path", value="/h/x")], False),
            ("a kde row a shipped file sets fails", [row(file="kwinrc", group=("Windows",), key="Placement",
                                                         value="Centered", kind="kde")], True),
            ("a kde row nothing sets passes", [row(file="kwinrc", group=("Windows",), key="BorderlessMaximizedWindows",
                                                   value="false", kind="kde")], False),
            ("a vm row reads the overlay", [row(file="kscreenlockerrc", group=("Daemon",), key="Autolock",
                                                value="false", kind="vm")], False),
            ("a live row reads the overlay", [row(file="baloofilerc", group=("Basic Settings",),
                                                  key="Indexing-Enabled", value="false", kind="live")], False),
            ("an installed row does not", [row(file="baloofilerc", group=("Basic Settings",),
                                               key="Indexing-Enabled", value="false")], True),
            ("a row nothing ships fails", [row(key="NoSuchKey")], True),
        ]
        for name, rows, should_fail in cases:
            failed = bool(run(rows))
            good = failed == should_fail
            ok &= good
            print("  %s %s" % ("ok  " if good else "FAIL", name))
    finally:
        shutil.rmtree(t, ignore_errors=True)
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description="every KDOS default reads back as its fixture table says")
    ap.add_argument("--root", help="a built tree; its etc/xdg and usr/share/kdos are read instead of fs/")
    ap.add_argument("--table", default=TABLE, help="the fixture (default %(default)s)")
    ap.add_argument("--kreadconfig", metavar="CMD", help="read through this kreadconfig6 command instead")
    ap.add_argument("--selftest", action="store_true", help="prove each kind of row can fail")
    a = ap.parse_args()

    if a.selftest:
        return selftest()
    if a.root and not os.path.isdir(os.path.join(a.root, "usr")):
        print("defaultscheck: %s is not a built tree" % a.root, file=sys.stderr)
        return 2
    rows, errors = load_table(a.table)
    if errors:
        for e in errors:
            print("defaultscheck: %s" % e, file=sys.stderr)
        return 2

    root, staged = a.root, None
    if not root:
        staged = tempfile.mkdtemp(prefix="defaultscheck-tree.")
        root = staged
    try:
        perrors = stage_tree(staged) if staged else []
        if a.kreadconfig:
            failures = check_kreadconfig(rows, root, a.kreadconfig)
        else:
            failures, rerrors = check_native(rows, root)
            perrors += rerrors
    finally:
        if staged:
            shutil.rmtree(staged, ignore_errors=True)
    if perrors:
        for e in perrors:
            print("defaultscheck: %s" % e, file=sys.stderr)
        return 2
    for f in failures:
        print("  FAIL  %s" % f)
    kinds = {}
    for r in rows:
        kinds[r["kind"]] = kinds.get(r["kind"], 0) + 1
    print("defaultscheck: %d rows (%s)%s; %d failing" % (
        len(rows), ", ".join("%d %s" % (kinds[k], k) for k in KINDS if k in kinds),
        " through " + a.kreadconfig.split()[0] if a.kreadconfig else "", len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
