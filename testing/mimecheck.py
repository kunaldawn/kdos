#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   mimecheck — every default application KDOS names is one the image installs
# ---------------------------------
#
#   python3 testing/mimecheck.py [--root build/fs] [--list FILE] [--strict]
#   python3 testing/mimecheck.py --ids org.kde.konsole.desktop [...]
#
# Run from the repository root. /usr/share/kdos/xdg/kde-mimeapps.list is the
# one list of KDOS's default applications, and the XDG lookup reads it before
# plasma-desktop's own. A desktop id in it that nothing installs is not an
# error anywhere at run time: the lookup skips it and the next association
# opens the file, so a wrong id ships as an application nobody chose.
#
# With --root the ids are looked for where the image has them,
# usr/share/applications/<id> (or the subdirectory the id's dashes name, as
# the desktop-entry specification allows). Without a built tree each id is
# traced to a port, the first of these that holds:
#
#   fs/        fs/usr/share/applications/<id>
#   src        a port of ours carries a file named <id> or <id>.in
#   build.sh   a recipe's build.sh names <id> (a heredoc, an install line)
#   tarball    a port the id names (org.kde.kate -> kate, okularApplication_pdf
#              -> okular, libreoffice-writer -> libreoffice) has <id>, its .in
#              or .cmake template, or for <port>-<x>.desktop an <x>.desktop,
#              in its fetched source — beside the recipe, or in the source
#              cache under its hash ($KDOS_SRCCACHE, ports/.srccache)
#
# An id whose candidate port is not fetched yet is UNVERIFIED: reported, and a
# failure only under --strict. An id no port accounts for fails.
#
# The list itself is held to its format: the three groups the specification
# defines, a MIME type or scheme handler as every key, a key once per group,
# and every value a ;-separated list of desktop ids.
#
# Exit 0 every id installed, 1 an id nothing installs, 2 a list that does not
# parse, 3 no list ships at all.

import argparse
import glob
import os
import re
import subprocess
import sys

LIST = "kde-mimeapps.list"

# Ids whose words do not name the port that installs them, measured from the
# port's source: the resolver adds the port to the id's candidates.
PROVIDERS = {
    "ktelnetservice6.desktop": "kio",
    "org.kde.krunner.desktop": "plasma-workspace",
}
GROUPS = ("Default Applications", "Added Associations", "Removed Associations")
MIME_KEY = re.compile(r"^[a-z0-9][a-z0-9.+-]*/[A-Za-z0-9][A-Za-z0-9.+_-]*$")


def find_lists(root):
    if root:
        cands = [os.path.join(root, "etc/xdg", LIST), os.path.join(root, "usr/share/kdos/xdg", LIST)]
    else:
        cands = ["fs/etc/xdg/" + LIST, "fs/usr/share/kdos/xdg/" + LIST]
        cands += sorted(glob.glob("src/*/*/**/xdg/" + LIST, recursive=True))
    return [p for p in cands if os.path.isfile(p)]


def parse_list(path):
    """{group: {mime: [ids]}} and a list of format errors."""
    groups, errors, cur = {}, [], None
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            s = raw.strip()
            if not s or s.startswith("#"):
                continue
            if s.startswith("[") and s.endswith("]"):
                cur = s[1:-1]
                if cur not in GROUPS:
                    errors.append("%s:%d: [%s] is not a group the specification defines" % (path, n, cur))
                groups.setdefault(cur, {})
                continue
            if cur is None:
                errors.append("%s:%d: an entry before any group" % (path, n))
                continue
            if "=" not in s:
                errors.append("%s:%d: not a key=value line: %r" % (path, n, s))
                continue
            key, val = (x.strip() for x in s.split("=", 1))
            if not MIME_KEY.match(key):
                errors.append("%s:%d: %r is not a MIME type" % (path, n, key))
            if key in groups[cur]:
                errors.append("%s:%d: %s is listed twice in [%s]; the later line wins silently"
                              % (path, n, key, cur))
            ids = [i for i in val.split(";") if i]
            for i in ids:
                if not i.endswith(".desktop") or "/" in i:
                    errors.append("%s:%d: %r is not a desktop id" % (path, n, i))
            groups[cur][key] = ids
    return groups, errors


# ── where an id is installed ─────────────────────────────────────────────

def installed_in_root(root, did):
    apps = os.path.join(root, "usr/share/applications")
    if os.path.isfile(os.path.join(apps, did)):
        return os.path.join("usr/share/applications", did)
    # The specification's subdirectory form: kde4-foo.desktop may be
    # applications/kde4/foo.desktop.
    parts = did.split("-")
    for i in range(1, len(parts)):
        rel = os.path.join("/".join(parts[:i]), "-".join(parts[i:]))
        if os.path.isfile(os.path.join(apps, rel)):
            return os.path.join("usr/share/applications", rel)
    return None


class Ports:
    """Every port by name, its directory, its build.sh text and, on demand,
    the member names of its fetched first source."""

    def __init__(self):
        self.dirs = {}
        for kp in sorted(glob.glob("ports/core/*/*/kpkgbuild") + glob.glob("src/*/*/kpkgbuild")):
            d = os.path.dirname(kp)
            self.dirs.setdefault(os.path.basename(d), d)
        self._builds = None
        self._members = {}

    def builds(self):
        if self._builds is None:
            self._builds = {}
            for name, d in self.dirs.items():
                p = os.path.join(d, "build.sh")
                try:
                    with open(p, encoding="utf-8", errors="replace") as f:
                        self._builds[name] = f.read()
                except OSError:
                    pass
        return self._builds

    def tarball(self, name):
        """The first source file the recipe hashes, when it is on disk: in
        the port directory, or in the source cache under its hash, where
        ports/fetch keeps it ($KDOS_SRCCACHE, ports/.srccache by default)."""
        d = self.dirs[name]
        cache = os.environ.get("KDOS_SRCCACHE") or "ports/.srccache"
        try:
            with open(os.path.join(d, "kpkgbuild"), encoding="utf-8") as f:
                for line in f:
                    m = re.match(r"sha256\s*=\s*([0-9a-f]{64})\s+(\S+)", line)
                    if m:
                        for p in (os.path.join(d, m.group(2)),
                                  os.path.join(cache, "sha256-" + m.group(1)[:2], m.group(1))):
                            if os.path.isfile(p):
                                return p
                        return ""
        except OSError:
            pass
        return None

    def members(self, name):
        """Basenames in the port's tarball; None when it is not fetched."""
        if name in self._members:
            return self._members[name]
        tb = self.tarball(name)
        names = None
        if tb:
            r = subprocess.run(["tar", "-tf", tb], capture_output=True, text=True, errors="replace")
            if r.returncode == 0:
                names = {os.path.basename(m.rstrip("/")) for m in r.stdout.splitlines()}
        self._members[name] = names
        return names


def candidates(did, ports):
    """Ports an id plausibly comes from, from its dotted, dashed and
    camelCase words: every run of adjacent words, joined by a dash or by
    nothing, that is a port's name."""
    stem = did[:-len(".desktop")] if did.endswith(".desktop") else did
    words = []
    for w in re.split(r"[._-]", stem):
        words += [p for p in re.findall(r"[A-Z]?[a-z0-9]+|[A-Z]+(?![a-z])", w)] or [w]
    words = [w.lower() for w in words if w]
    out = []
    for i in range(len(words)):
        for j in range(i + 1, len(words) + 1):
            for joined in ("-".join(words[i:j]), "".join(words[i:j])):
                if joined in ports.dirs and joined not in out:
                    out.append(joined)
    whole = stem.lower()
    if whole in ports.dirs and whole not in out:
        out.insert(0, whole)
    known = PROVIDERS.get(did)
    if known in ports.dirs and known not in out:
        out.append(known)
    # The longest name first: kiwix-desktop before kiwix.
    return sorted(out, key=lambda n: -len(n))


def resolve(did, ports):
    """(state, port, evidence): state is ok, unverified or missing."""
    if os.path.isfile(os.path.join("fs/usr/share/applications", did)):
        return "ok", "fs", "fs/usr/share/applications/" + did
    for name, d in ports.dirs.items():
        if not d.startswith("src/"):
            continue
        for p in glob.glob(os.path.join(d, "**", did), recursive=True) + \
                glob.glob(os.path.join(d, "**", did + ".in"), recursive=True):
            return "ok", name, p
    cands = candidates(did, ports)
    builds = ports.builds()
    # The id as a whole word: kate.desktop inside org.kde.kate.desktop is
    # another application's entry, and would pass an id nothing installs.
    word = re.compile(r"(?<![\w.-])%s(?![\w.-])" % re.escape(did))
    for name in cands + [n for n in builds if n not in cands]:
        text = builds.get(name, "")
        if word.search(text):
            return "ok", name, os.path.join(ports.dirs[name], "build.sh")
    stem = did[:-len(".desktop")]
    wanted = {did, did + ".in", did + ".cmake", stem + ".desktop.in", stem + ".desktop.cmake",
              stem + ".desktop.in.in"}
    unfetched = None
    for name in cands:
        members = ports.members(name)
        if members is None:
            unfetched = unfetched or name
            continue
        hit = wanted & members
        if hit:
            return "ok", name, "%s source: %s" % (name, sorted(hit)[0])
        if did.startswith(name + "-"):
            inner = did[len(name) + 1:]
            if inner in members:
                return "ok", name, "%s source: %s, installed as %s" % (name, inner, did)
    if unfetched:
        return "unverified", unfetched, "its source is not fetched (ports/fetch %s)" % unfetched
    return "missing", None, "no port installs it" + (" (looked in: %s)" % ", ".join(cands) if cands else "")


def main():
    ap = argparse.ArgumentParser(description="every default application KDOS names is installed")
    ap.add_argument("--root", help="a built tree; ids are looked for in its usr/share/applications")
    ap.add_argument("--list", action="append", default=[], help="the list to check (repeatable)")
    ap.add_argument("--ids", nargs="+", help="check these desktop ids instead of a list")
    ap.add_argument("--strict", action="store_true", help="an unverified id fails")
    ap.add_argument("--quiet", action="store_true", help="print only what fails")
    a = ap.parse_args()

    if a.root and not os.path.isdir(os.path.join(a.root, "usr")):
        print("mimecheck: %s is not a built tree" % a.root, file=sys.stderr)
        return 2

    uses = {}
    if a.ids:
        for i in a.ids:
            uses.setdefault(i, []).append("named on the command line")
    else:
        lists = a.list or find_lists(a.root)
        if not lists:
            print("mimecheck: no %s ships (looked in %s)" %
                  (LIST, "the tree's etc/xdg and usr/share/kdos/xdg" if a.root
                   else "fs/ and src/*/*/**/xdg"), file=sys.stderr)
            return 3
        errors = []
        for path in lists:
            groups, errs = parse_list(path)
            errors += errs
            for group in ("Default Applications", "Added Associations"):
                for mime, ids in groups.get(group, {}).items():
                    for i in ids:
                        uses.setdefault(i, []).append("%s [%s] %s" % (path, group, mime))
        if errors:
            for e in errors:
                print("mimecheck: %s" % e, file=sys.stderr)
            return 2

    ports = None if a.root else Ports()
    missing = unverified = 0
    for did in sorted(uses):
        if a.root:
            where = installed_in_root(a.root, did)
            state, evidence = ("ok", where) if where else ("missing", "not in usr/share/applications")
        else:
            state, _port, evidence = resolve(did, ports)
        first = uses[did][0] + (" and %d more" % (len(uses[did]) - 1) if len(uses[did]) > 1 else "")
        if state == "missing":
            missing += 1
            print("  FAIL  %s: %s — %s" % (did, evidence, first))
        elif state == "unverified":
            unverified += 1
            print("  %s  %s: %s — %s" % ("FAIL" if a.strict else "note", did, evidence, first))
        elif not a.quiet:
            print("  ok    %s: %s" % (did, evidence))
    if not a.quiet:
        print("mimecheck: %d desktop ids, %d installed by nothing, %d unverified%s" %
              (len(uses), missing, unverified, " (built tree)" if a.root else ""))
    return 1 if missing or (a.strict and unverified) else 0


if __name__ == "__main__":
    sys.exit(main())
