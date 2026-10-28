#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   kfscheck — KDOS from Scratch against the tree it is rendered from
# ---------------------------------
#
#     kfscheck.py [--root DIR] [--base REF] [--strict] [--only CHECK,…]
#     kfscheck.py [--root DIR] --repin
#
# Run from the repository root (or give --root). Exit status 1 on any
# failure. The checks, each one name for --only:
#
#   sidecars   every kfs.md beside a recipe (ports/core/<shelf>/<name>/ and
#              src/<area>/<name>/) has only the sections About, Flags,
#              Patches and Notes; every `## Flags` bullet opens with a
#              backquoted flag that the port's build.sh still names whole
#              (`-DCMAK` is not named by `-DCMAKE_BUILD_TYPE=…`, nor
#              `--foo=1` by `--foo=2`; a bullet with no `=` matches the
#              flag with any value), and
#              every `## Patches` bullet with a backquoted file that is in
#              the port's directory. A note about a flag the recipe dropped
#              is a page explaining a build that does not happen.
#   notes      a NEW OR CHANGED port that meets a Tier B criterion (a patch,
#              vendored dependencies, a build.sh of more than 30 lines, 20 or
#              more reverse dependencies: script/book/kfsgen.py's
#              assign_tiers, the rule the renderer uses) has a kfs.md with an
#              About or a Notes section (kfsgen's has_note, the same rule
#              that keeps "none yet" off its page).
#   patches    a NEW OR CHANGED .patch or .diff under ports/ or src/ has a
#              `Subject:` line in its mail header, before the first `diff `,
#              `--- ` or `Index: ` line: the header kfsgen prints as what
#              the patch does, and a page shows "undescribed" without it.
#   snapshot   counts the ports whose version-release in
#              docs/kfs/data/packages.tsv is not the recipe's, which render
#              with "snapshot predates this version". Fails only with
#              --strict: a recipe bump is not a reason to rebuild the tree.
#   stamp      docs/kfs/data/recipes.stamp holds kfsgen's stamp_digest of
#              this tree: every recipe, every kfs.md, the phase lists and
#              environments, the bootstrap step scripts, ports/shelves,
#              ports/sources.idx, testing/optdeps.py, fs/etc/os-release and
#              every kfs:include target, less the ports that link a book
#              under docs/ (kfsgen's stamp_inputs and stamp_digest say
#              why). A change to any of them changes the
#              digest, so a stale stamp fails until `make book-stamp` (or
#              `make book-data`) rewrites it, which makes the change also a
#              change to docs/kfs and to the kdos-kfs port that ships the
#              book.
#   includes   every `kfs:include` in a docs/kfs page, outside a fence,
#              names a file in the tree, and its `lines=A-B`, if any, lies
#              inside that file and carries `pin=`, the range_pin of the
#              lines the page was written against, which still matches.
#              When the pinned lines are elsewhere in the file, the failure
#              says where; `--repin` then rewrites each such directive to
#              the range they moved to, and pins every range that has no
#              pin yet. A range whose lines changed is never repinned: the
#              prose around it explains the old lines, so read it first and
#              repin by deleting the stale pin.
#   kernel     every table row of docs/kfs/14-kernel/configuring.md whose
#              first cell is a backquoted `CONFIG_…` symbol states the value
#              the linux port's kdos.config gives it. The row is
#              | `CONFIG_FOO` | location | `value` | use |, and the value is
#              `y`, `m`, `n` for a `# CONFIG_FOO is not set` line, or the
#              text after `=` exactly, quotes kept (`"kdos"`). A symbol
#              kdos.config does not set fails too: its value is the
#              kernel's default, which no build checks, so the row could
#              go false with no file in the tree changing. A repeated
#              symbol fails, and so does a page with no rows at all.
#   render     kfsgen renders the whole book source into a temporary
#              directory with every port placed and no warning: a warning
#              is a link the rendered book prints as plain text, or a name
#              that is not a port.
#
# NEW OR CHANGED means not yet on any remote: the paths every commit on HEAD
# that no refs/remotes/* ref holds adds or modifies, plus the working tree's
# changes against HEAD and its untracked files. A move that leaves the content
# whole (git's R100) is not a change, so reshelving a port does not make its
# old patches new; but a path that is already new or changed stays so when a
# later commit moves it, which is why the commits are read oldest first. --base REF, or KDOS_CHECK_BASE in the environment, takes
# the working tree's difference from REF instead. Outside a git checkout, or
# with no remote-tracking ref and no base, nothing can be called new: the
# two gated checks print how many ports and patches they would refuse and
# pass. testing/preflight.sh runs `--only patches`, and script/hooks/pre-push
# applies the same Subject rule to the pushed commits itself.

import argparse
import importlib.util
import os
import re
import subprocess
import sys
import tempfile

SECTIONS = {"about", "flags", "patches", "notes"}
CHECKS = ["sidecars", "notes", "patches", "snapshot", "stamp", "includes", "kernel", "render"]
PATCH_RE = re.compile(r"\.(patch|diff)$")


def load_kfsgen(root):
    # No __pycache__ beside kfsgen.py: script/book ships inside kdos-kfs,
    # and the checkout should hold only the files it commits. Both recipe
    # hashes skip a cache that does get written.
    sys.dont_write_bytecode = True
    path = os.path.join(root, "script/book/kfsgen.py")
    spec = importlib.util.spec_from_file_location("kfsgen", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def git(root, *args):
    r = subprocess.run(["git", "-C", root] + list(args), capture_output=True, text=True)
    return r.returncode, r.stdout


def name_status(text, out=None):
    """Paths added or modified, from `--name-status -M` output read in
    order, oldest change first, into the set `out`. A rename or copy counts
    as its new path unless the content is whole (R100, C100), and then
    only when its old path was already in `out`: a patch added and then
    moved whole is still new. A rename takes its old path out, and so does
    a delete."""
    out = set() if out is None else out
    for line in text.split("\n"):
        f = line.split("\t")
        if len(f) < 2 or not f[0]:
            continue
        k = f[0][0]
        if k in "AM":
            out.add(f[1])
        elif k == "D":
            out.discard(f[1])
        elif k in "RC" and len(f) >= 3:
            if f[0] != k + "100" or f[1] in out:
                out.add(f[2])
            if k == "R":
                out.discard(f[1])
    return out


def changed_paths(root, base):
    """(set of paths, what they were measured against), or (None, why)."""
    rc, top = git(root, "rev-parse", "--show-toplevel")
    if rc != 0:
        return None, "not a git checkout"
    paths = set()
    if base:
        rc, out = git(root, "diff", "--name-status", "-M", base, "--")
        if rc != 0:
            return None, "base %s does not resolve" % base
        name_status(out, paths)
        against = base
    else:
        rc, refs = git(root, "for-each-ref", "--format=%(refname)", "refs/remotes/")
        if rc != 0 or not refs.strip():
            return None, "no remote-tracking ref to measure against"
        rc, out = git(root, "log", "--reverse", "--format=", "--name-status", "-M", "HEAD",
                      "--not", "--remotes")
        if rc != 0:
            return None, "git log failed"
        name_status(out, paths)
        rc, out = git(root, "diff", "--name-status", "-M", "HEAD", "--")
        name_status(out, paths)
        against = "every remote-tracking ref"
    rc, out = git(root, "ls-files", "--others", "--exclude-standard", "--", "ports", "src")
    paths |= {p for p in out.split("\n") if p}
    return {p for p in paths if os.path.lexists(os.path.join(root, p))}, against


class Report:
    def __init__(self):
        self.fails = []
        self.notes = []

    def fail(self, kind, msg):
        self.fails.append("%-9s %s" % (kind, msg))

    def note(self, msg):
        self.notes.append(msg)


def names_flag(build, flag):
    """Does build.sh name this flag whole? Not inside a longer word before
    it, not as the prefix of a longer flag after it, and, when the flag has
    a value, not as the prefix of a longer value."""
    tail = r"(?![^\s\\'\";)|&])" if "=" in flag else r"(?![\w.+-])"
    return re.search(r"(?<![\w$/.-])" + re.escape(flag) + tail, build) is not None


def first_code(bullet):
    m = re.search(r"`([^`]+)`", bullet)
    return m.group(1) if m else None


def check_sidecars(root, kg, ports, rep):
    n = 0
    for p in sorted(ports.values(), key=lambda p: p.rel):
        path = os.path.join(p.dir, "kfs.md")
        if not os.path.isfile(path):
            continue
        n += 1
        rel = p.rel + "/kfs.md"
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        sect, fen, seen = None, False, set()
        for ln, line in enumerate(text.split("\n"), 1):
            if kg.FENCE_RE.match(line):
                fen = not fen
                continue
            if fen:
                continue
            h = re.match(r"^(#+)\s", line)
            if h and h.group(1) != "##":
                rep.fail("SIDECAR", "%s:%d: a heading in a note is `## `, one of About, Flags, "
                         "Patches or Notes" % (rel, ln))
                continue
            m = re.match(r"^##\s+(.*?)\s*$", line)
            if m:
                sect = m.group(1).lower()
                if sect not in SECTIONS:
                    rep.fail("SIDECAR", "%s:%d: section '%s' is not About, Flags, Patches or Notes"
                             % (rel, ln, m.group(1)))
                elif sect in seen:
                    rep.fail("SIDECAR", "%s:%d: section '%s' appears twice; the second would "
                             "replace the first" % (rel, ln, m.group(1)))
                seen.add(sect)
                continue
            if not re.match(r"^[-*]\s", line) or sect not in ("flags", "patches"):
                continue
            what = first_code(line)
            if what is None:
                rep.fail("SIDECAR", "%s:%d: a %s bullet must open with the backquoted %s"
                         % (rel, ln, sect.capitalize(), "flag" if sect == "flags" else "patch"))
                continue
            if sect == "flags":
                if not names_flag(p.build or "", what):
                    rep.fail("SIDECAR", "%s:%d: flag %s does not occur in %s/build.sh"
                             % (rel, ln, what, p.rel))
            elif not os.path.isfile(os.path.join(p.dir, what)):
                rep.fail("SIDECAR", "%s:%d: patch %s is not in %s" % (rel, ln, what, p.rel))
            elif not re.match(r"^-\s+`[^`]+`\s+—\s+\S", line):
                rep.fail("SIDECAR", "%s:%d: a Patches bullet is one line, `- `file` — text`, "
                         "the one form kfsgen reads" % (rel, ln))
            elif any(f == what and subj for f, subj, _ in p.patches):
                rep.fail("SIDECAR", "%s:%d: %s has its own Subject:, which the page shows "
                         "instead of this bullet" % (rel, ln, what))
    rep.note("sidecars: %d kfs.md checked" % n)


def port_of(ports_by_rel, path):
    parts = path.split("/")
    for k in (4, 3):
        rel = "/".join(parts[:k])
        if rel in ports_by_rel:
            return ports_by_rel[rel]
    return None


def check_notes(root, kg, ports, data, changed, against, rep):
    kg.assign_tiers(ports, data)
    need = [p for p in ports.values() if p.reasons]
    if changed is None:
        missing = sum(1 for p in need if not kg.has_note(p))
        rep.note("notes: no base (%s); %d ports meet a Tier B criterion, %d of them without a "
                 "kfs.md note" % (against, len(need), missing))
        return
    by_rel = {p.rel: p for p in ports.values()}
    touched = {port_of(by_rel, c) for c in changed} - {None}
    gated = [p for p in need if p in touched]
    for p in sorted(gated, key=lambda p: p.name):
        if not kg.has_note(p):
            rep.fail("NOTE", "%s: new or changed, and %s, so it needs %s/kfs.md with an "
                     "About or a Notes section" % (p.name, "; ".join(p.reasons), p.rel))
    rep.note("notes: %d new or changed ports since %s, %d of them meeting a Tier B criterion"
             % (len(touched), against, len(gated)))


def all_patches(root):
    out = []
    for top in ("ports", "src"):
        for dp, dn, fn in os.walk(os.path.join(root, top)):
            dn[:] = [d for d in dn if not d.startswith(".")]
            for f in fn:
                if PATCH_RE.search(f):
                    out.append(os.path.relpath(os.path.join(dp, f), root))
    return sorted(out)


def check_patches(root, kg, changed, against, rep):
    if changed is None:
        bare = [p for p in all_patches(root)
                if kg.parse_patch(os.path.join(root, p))[0] is None]
        rep.note("patches: no base (%s); %d patches in the tree, %d without a Subject: header"
                 % (against, len(all_patches(root)), len(bare)))
        return
    gated = sorted(p for p in changed if PATCH_RE.search(p) and p.split("/", 1)[0] in ("ports", "src")
                   and os.path.isfile(os.path.join(root, p)))
    for p in gated:
        subj, _ = kg.parse_patch(os.path.join(root, p))
        if not subj:
            rep.fail("PATCHHDR", "%s: new or changed, and has no Subject: line before its first "
                     "diff — say in one line what it changes" % p)
    rep.note("patches: %d new or changed since %s" % (len(gated), against))


def check_snapshot(root, ports, data, strict, rep):
    stale, absent = [], 0
    for n, p in sorted(ports.items()):
        row = data.packages.get(n)
        if not row:
            absent += 1
            continue
        if len(row) >= 3 and (row[1], row[2]) != (p.decl["version"], p.decl["release"]):
            stale.append("%s %s-%s (recipe %s-%s)" % (n, row[1], row[2], p.decl["version"],
                                                      p.decl["release"]))
    extra = sum(1 for n in data.packages if n not in ports)
    msg = ("snapshot: %d of %d ports in it, %d predate their recipe, %d ports not in it, "
           "%d names in it with no recipe (taken at %s)"
           % (len(ports) - absent, len(ports), len(stale), absent, extra,
              data.stamp.get("commit", "unknown")))
    if strict and stale:
        for s in stale[:20]:
            rep.fail("SNAPSHOT", s)
        rep.fail("SNAPSHOT", "%d ports predate their recipe — make book-data BUILDFS=<a built tree>"
                 % len(stale))
    rep.note(msg)


def check_stamp(root, kg, data_dir, rep):
    path = os.path.join(data_dir, "recipes.stamp")
    want = None
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                w = line.split()
                if len(w) == 2 and w[0] == "sha256":
                    want = w[1]
    except OSError:
        rep.fail("STAMP", "%s is missing — run make book-stamp" % os.path.relpath(path, root))
        return
    got, n, files = kg.stamp_digest(root)
    if want != got:
        rep.fail("STAMP", "%s is stale: the tree digests to %s, it says %s — a recipe, a "
                 "kfs.md, a phase list or an included file changed; run make book-stamp "
                 "(no built tree needed), or make book-data BUILDFS=<a built tree> after a build"
                 % (os.path.relpath(path, root), got[:12], (want or "nothing")[:12]))
    else:
        rep.note("stamp: current over %d recipes and %d other files" % (n, files))


def check_includes(root, kg, rep, repin=False):
    """With repin, rewrite the directives that can be repinned (see the
    header) and report only what is left."""
    book = os.path.join(root, "docs/kfs")
    n, moved, pinned = 0, 0, 0
    for dp, dn, fn in os.walk(book):
        dn.sort()
        for f in sorted(fn):
            if not f.endswith(".md"):
                continue
            path = os.path.join(dp, f)
            rel = os.path.relpath(path, root)
            fen, edits = False, {}
            with open(path, encoding="utf-8", errors="replace") as fh:
                page = fh.read().split("\n")
            for ln, line in enumerate(page, 1):
                if kg.FENCE_RE.match(line):
                    fen = not fen
                    continue
                m = kg.DIRECTIVE.match(line)
                if fen or not m or m.group(1) != "include":
                    continue
                n += 1
                name, rng, pin, bad = kg.include_args(m.group(2))
                if not name:
                    rep.fail("INCLUDE", "%s:%d: kfs:include names no file" % (rel, ln))
                    continue
                for a in bad:
                    rep.fail("INCLUDE", "%s:%d: '%s' is not lines=A-B or pin=HASH" % (rel, ln, a))
                target = os.path.join(root, name)
                if not os.path.isfile(target):
                    rep.fail("INCLUDE", "%s:%d: %s does not exist" % (rel, ln, name))
                    continue
                if not rng:
                    if pin:
                        rep.fail("INCLUDE", "%s:%d: pin= without lines=" % (rel, ln))
                    continue
                with open(target, encoding="utf-8", errors="replace") as tf:
                    lines = tf.read().split("\n")
                count = len(lines) - (1 if lines and lines[-1] == "" else 0)
                lo, hi = rng
                inside = 1 <= lo <= hi <= count
                want = "lines=%d-%d" % (lo, hi)
                if pin and inside and kg.range_pin(lines, lo, hi) == pin:
                    continue
                at = kg.find_pin(lines, hi - lo + 1, pin) if pin else None
                if repin and not pin and inside:
                    new = kg.range_pin(lines, lo, hi)
                    edits[ln] = line.replace(want, "%s pin=%s" % (want, new), 1)
                    pinned += 1
                elif repin and at:
                    edits[ln] = line.replace(want, "lines=%d-%d" % (at, at + hi - lo), 1)
                    moved += 1
                elif not inside:
                    rep.fail("INCLUDE", "%s:%d: lines=%d-%d, and %s has %d lines%s"
                             % (rel, ln, lo, hi, name, count,
                                "; the pinned lines are now %d-%d" % (at, at + hi - lo) if at else ""))
                elif not pin:
                    rep.fail("INCLUDE", "%s:%d: %s %s has no pin= — check the lines are the "
                             "ones the page explains, then kfscheck.py --repin" % (rel, ln, name, want))
                elif at:
                    rep.fail("INCLUDE", "%s:%d: %s %s: the pinned lines are now %d-%d — "
                             "kfscheck.py --repin moves the range"
                             % (rel, ln, name, want, at, at + hi - lo))
                else:
                    rep.fail("INCLUDE", "%s:%d: %s %s: the pinned lines changed — read the "
                             "prose against the file, fix the range and delete the pin, then "
                             "kfscheck.py --repin" % (rel, ln, name, want))
            if edits:
                for ln, text in edits.items():
                    page[ln - 1] = text
                with open(path, "w", encoding="utf-8") as fh:
                    fh.write("\n".join(page))
    rep.note("includes: %d resolved%s" % (n, ", %d repinned, %d moved" % (pinned, moved)
                                          if repin else ""))


KERNEL_PAGE = "docs/kfs/14-kernel/configuring.md"
KCONFIG_LINE = re.compile(r"^(?:(CONFIG_[A-Za-z0-9_]+)=(.*)|# (CONFIG_[A-Za-z0-9_]+) is not set)$")
KERNEL_ROW = re.compile(r"^\|\s*`(CONFIG_[A-Za-z0-9_]+)`\s*\|")


def table_cells(line):
    """The cells of one Markdown table row, split on every `|` that is not
    escaped and not inside a code span."""
    cells, cur, code, i = [], "", False, 0
    s = line.strip()
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            cur += s[i:i + 2]
            i += 2
            continue
        if c == "`":
            code = not code
        if c == "|" and not code:
            cells.append(cur.strip())
            cur = ""
        else:
            cur += c
        i += 1
    cells.append(cur.strip())
    return cells[1:-1] if len(cells) >= 2 else cells


def read_kdos_config(path):
    """{symbol: value} from a kdos.config: the text after `=`, quotes kept,
    or `n` for a `# CONFIG_… is not set` line."""
    out = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = KCONFIG_LINE.match(line.rstrip("\n"))
            if m:
                out[m.group(1) or m.group(3)] = "n" if m.group(3) else m.group(2)
    return out


def check_kernel(root, kg, ports, rep):
    page = os.path.join(root, KERNEL_PAGE)
    lnx = ports.get("linux")
    if lnx is None:
        rep.fail("KERNEL", "no linux port in the tree to check %s against" % KERNEL_PAGE)
        return
    conf_path = os.path.join(lnx.dir, "kdos.config")
    conf_rel = os.path.relpath(conf_path, root)
    if not os.path.isfile(conf_path):
        rep.fail("KERNEL", "%s does not exist" % conf_rel)
        return
    if not os.path.isfile(page):
        rep.fail("KERNEL", "%s does not exist" % KERNEL_PAGE)
        return
    conf = read_kdos_config(conf_path)
    with open(page, encoding="utf-8", errors="replace") as f:
        lines = f.read().split("\n")
    seen, rows, fen = {}, 0, False
    for ln, line in enumerate(lines, 1):
        if kg.FENCE_RE.match(line):
            fen = not fen
            continue
        if fen:
            continue
        m = KERNEL_ROW.match(line)
        if not m:
            # A table row that names an option in its first cell in any
            # other form would drop out of the comparison unseen.
            first = table_cells(line)[:1] if line.lstrip().startswith("|") else []
            if first and "CONFIG_" in first[0]:
                rep.fail("KERNEL", "%s:%d: an option row starts `| `CONFIG_…` |` at column 0, "
                         "the option alone in backquotes; this one is '%s'"
                         % (KERNEL_PAGE, ln, first[0]))
            continue
        rows += 1
        sym = m.group(1)
        where = "%s:%d: %s" % (KERNEL_PAGE, ln, sym)
        if sym in seen:
            rep.fail("KERNEL", "%s is already the row at line %d" % (where, seen[sym]))
            continue
        seen[sym] = ln
        cells = table_cells(line)
        if len(cells) < 4:
            rep.fail("KERNEL", "%s: a row has four cells, option, location, value and use; "
                     "this one has %d" % (where, len(cells)))
            continue
        v = re.match(r"^`([^`]+)`$", cells[2])
        if not v:
            rep.fail("KERNEL", "%s: the value cell must be one backquoted value, `y`, `m`, `n` "
                     "or the text after `=` in kdos.config; it is '%s'" % (where, cells[2]))
            continue
        if sym not in conf:
            rep.fail("KERNEL", "%s: %s does not set it, so its value is the kernel's default, "
                     "which no build checks — name it in the prose instead" % (where, conf_rel))
        elif conf[sym] != v.group(1):
            rep.fail("KERNEL", "%s: the page says %s, %s says %s"
                     % (where, v.group(1), conf_rel, conf[sym]))
    if not rows:
        rep.fail("KERNEL", "%s has no option rows (| `CONFIG_…` | location | `value` | use |)"
                 % KERNEL_PAGE)
    else:
        rep.note("kernel: %d option rows checked against %s" % (rows, conf_rel))


def check_render(root, rep):
    with tempfile.TemporaryDirectory(prefix="kfscheck.") as tmp:
        r = subprocess.run([sys.executable, os.path.join(root, "script/book/kfsgen.py"),
                            "--root", root, "--out", os.path.join(tmp, "src")],
                           capture_output=True, text=True)
    warns = [ln for ln in r.stderr.split("\n") if ln.startswith("kfsgen: warning: ")]
    for w in warns:
        rep.fail("RENDER", w[len("kfsgen: warning: "):])
    if r.returncode != 0:
        rep.fail("RENDER", "kfsgen exited %d: %s" % (r.returncode,
                                                     (r.stdout + r.stderr).strip().split("\n")[-1]))
    elif not warns:
        rep.note("render: " + r.stdout.strip().split("\n")[-1].replace("kfsgen: ", ""))


def main():
    ap = argparse.ArgumentParser(description="check KDOS from Scratch against the tree")
    ap.add_argument("--root", default=None)
    ap.add_argument("--base", default=os.environ.get("KDOS_CHECK_BASE") or None)
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--only", default=",".join(CHECKS))
    ap.add_argument("--repin", action="store_true",
                    help="pin unpinned include ranges, move moved ones, then check includes")
    a = ap.parse_args()
    root = os.path.abspath(a.root or os.getcwd())
    only = ["includes"] if a.repin else [c for c in a.only.split(",") if c]
    for c in only:
        if c not in CHECKS:
            ap.error("--only %s: not one of %s" % (c, ", ".join(CHECKS)))
    kg = load_kfsgen(root)
    data_dir = os.path.join(root, "docs/kfs/data")
    data = kg.Data(data_dir)
    rep = Report()
    ports = kg.read_ports(root) if set(only) & {"sidecars", "notes", "snapshot", "kernel"} else {}
    changed, against = (None, "")
    if set(only) & {"notes", "patches"}:
        changed, against = changed_paths(root, a.base)
    for c in only:
        if c == "sidecars":
            check_sidecars(root, kg, ports, rep)
        elif c == "notes":
            check_notes(root, kg, ports, data, changed, against, rep)
        elif c == "patches":
            check_patches(root, kg, changed, against, rep)
        elif c == "snapshot":
            check_snapshot(root, ports, data, a.strict, rep)
        elif c == "stamp":
            check_stamp(root, kg, data_dir, rep)
        elif c == "includes":
            check_includes(root, kg, rep, a.repin)
        elif c == "kernel":
            check_kernel(root, kg, ports, rep)
        elif c == "render":
            check_render(root, rep)
    for n in rep.notes:
        print("  " + n)
    for f in rep.fails:
        print(f)
    if rep.fails:
        print("kfscheck: %d problem(s)" % len(rep.fails))
        return 1
    print("kfscheck: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
