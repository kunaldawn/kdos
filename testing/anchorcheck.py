#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   anchorcheck — every #fragment link in the two books lands on a heading
# ---------------------------------
#
#     anchorcheck.py [--root DIR] [--book DIR]... [FILE]...
#
# With no --book and no FILE it reads every page of docs/kdos and docs/kfs and
# the repository README.md; --book narrows it to one book (repeatable), and a
# FILE checks only that page's links. Exit status 1 when any link fails.
#
# A link is checked when it carries a fragment: `#x` on its own page,
# `page.md#x` on another page of either book, or `x.md#x` on any Markdown file
# in the tree. The fragment must be an id the target page has, as GitHub
# gives it: the heading's rendered text, lowercased, every character that is
# not a letter, digit, `_`, `-` or space dropped, each space a `-`, and a
# repeated id suffixed -1, -2 in page order. An explicit `<a id="x">` or
# `<a name="x">` is an id too. A heading's trailing `{…}` attribute block,
# which pandoc reads and GitHub prints, is not part of its text. The rule is
# script/book/kfsgen.py's github_anchor and kdos-doc's mkbook.py anchor(); a
# change to one is a change to all three, or the book builds links this
# check calls broken.
#
# docscheck checks that a link's FILE exists and not its fragment, so a
# renamed heading breaks every link to it with nothing else noticing. A link
# into a heading that a page carries under a fence is not a heading: fences
# and inline code are skipped on both sides.
#
# THE PACKAGE PAGES OF docs/kfs ARE GENERATED and never committed, so their
# anchors are not in any file. On a docs/kfs page a bare `#port-<name>` must
# name a recipe (`+` spelled `x`, as kfsgen spells it), `#phase-<name>` a
# directory under script/phases and `#shelf-<phase>-<shelf>` a phase and a
# line of ports/shelves. The same forms with a page in front, or on a
# docs/kdos page, are refused: kfsgen joins a page and its fragment, and
# GitHub has nowhere to send either.

import argparse
import os
import re
import sys

FENCE_RE = re.compile(r"^\s*(```+|~~~+)")
HEAD_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*$")
ATTR_RE = re.compile(r"\s*\{[^}]*\}\s*$")
HTML_ID_RE = re.compile(r"<a\s+(?:[^>]*\s)?(?:id|name)=\"([^\"]+)\"", re.I)
LINK_RE = re.compile(r"(!?)\[((?:[^\[\]]|\[[^\]]*\])*)\]\(\s*(<[^>]*>|[^()\s]+)(?:\s+\"[^\"]*\")?\s*\)")
CODE_RE = re.compile(r"(`+)(?:(?!\1).)+?\1")
SCHEME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9+.-]*:")
BOOKS = ["docs/kdos", "docs/kfs"]


def github_anchor(text):
    t = re.sub(r"!?\[((?:[^\[\]]|\[[^\]]*\])*)\]\([^)]*\)", r"\1", text)
    t = t.replace("`", "").replace("**", "").replace("*", "")
    t = t.strip().lower()
    t = re.sub(r"[^\w\- ]", "", t)
    return t.replace(" ", "-")


def prose_lines(text):
    """(line number, line) outside fences, with inline code blanked."""
    fen = None
    for i, line in enumerate(text.split("\n"), 1):
        m = FENCE_RE.match(line)
        if m:
            mark = m.group(1)[0]
            if fen is None:
                fen = mark
            elif fen == mark:
                fen = None
            continue
        if fen is None:
            yield i, CODE_RE.sub(lambda c: " " * len(c.group(0)), line)


_ids = {}


def ids_of(path):
    if path in _ids:
        return _ids[path]
    out, seen = set(), {}
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError:
        _ids[path] = None
        return None
    fen = None
    for line in text.split("\n"):
        m = FENCE_RE.match(line)
        if m:
            mark = m.group(1)[0]
            fen = mark if fen is None else (None if fen == mark else fen)
            continue
        if fen is not None:
            continue
        out.update(HTML_ID_RE.findall(line))
        m = HEAD_RE.match(line)
        if not m:
            continue
        h = ATTR_RE.sub("", m.group(2))
        h = re.sub(r"\s+#+$", "", h)
        a = github_anchor(h)
        n = seen.get(a, 0)
        seen[a] = n + 1
        out.add(a if not n else "%s-%d" % (a, n))
    _ids[path] = out
    return out


class Generated:
    """The anchors kfsgen gives generated pages, from the tree."""

    def __init__(self, root):
        self.ports, self.phases, self.shelves = set(), set(), set()
        for top in ["ports/core"] + ["src/" + a for a in ("system", "art", "desktop", "daemons", "devtools")]:
            base = os.path.join(root, top)
            if not os.path.isdir(base):
                continue
            for dp, dn, fn in os.walk(base):
                if "kpkgbuild" in fn:
                    self.ports.add("port-" + os.path.basename(dp).replace("+", "x"))
                    dn[:] = []
                dn[:] = [d for d in dn if not d.startswith(".")]
        ph = os.path.join(root, "script/phases")
        if os.path.isdir(ph):
            self.phases = {d for d in os.listdir(ph) if os.path.isdir(os.path.join(ph, d))}
        try:
            with open(os.path.join(root, "ports/shelves"), encoding="utf-8") as f:
                for line in f:
                    w = line.split("#", 1)[0].split()
                    if w:
                        self.shelves.add(w[0])
        except OSError:
            pass
        self.shelves |= {"src-" + a for a in ("system", "art", "desktop", "daemons")}

    def why_not(self, frag):
        """None when the anchor exists, else what is wrong."""
        if frag.startswith("port-"):
            return None if frag in self.ports else "no recipe is named %s" % frag[5:]
        if frag.startswith("phase-"):
            return None if frag[6:] in self.phases else "no phase %s" % frag[6:]
        if frag == "shelf-unphased":
            return None
        for p in sorted(self.phases, key=len, reverse=True):
            if frag.startswith("shelf-%s-" % p):
                s = frag[len("shelf-%s-" % p):]
                return None if s in self.shelves else "no shelf %s" % s
        return "no phase in %s" % frag


def generated_form(frag):
    return frag.startswith(("port-", "phase-", "shelf-"))


def check_page(root, rel, gen, bad):
    path = os.path.join(root, rel)
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    in_kfs = rel.startswith("docs/kfs/")
    here = os.path.dirname(path)
    for ln, line in prose_lines(text):
        for m in LINK_RE.finditer(line):
            target = m.group(3)
            if target.startswith("<"):
                target = target[1:-1]
            if SCHEME_RE.match(target) or "#" not in target:
                continue
            fpath, _, frag = target.partition("#")
            if not frag:
                continue
            if not fpath:
                tpath = path
            else:
                tpath = os.path.normpath(os.path.join(here, fpath))
                if os.path.isdir(tpath):
                    tpath = os.path.join(tpath, "README.md")
                if not tpath.endswith(".md"):
                    continue
            where = "%s:%d" % (rel, ln)
            bad.checked += 1
            if generated_form(frag) and frag not in (ids_of(tpath) or ()):
                if in_kfs and not fpath:
                    why = gen.why_not(frag)
                    if why:
                        bad.append("%s -> %s (%s)" % (where, target, why))
                else:
                    bad.append("%s -> %s (a generated package-page anchor works only as a bare "
                               "#fragment on a docs/kfs page)" % (where, target))
                continue
            ids = ids_of(tpath)
            if ids is None:
                bad.append("%s -> %s (no such file)" % (where, target))
            elif frag not in ids:
                bad.append("%s -> %s (%s has no heading with that id)"
                           % (where, target, os.path.relpath(tpath, root)))


class Found(list):
    checked = 0


def main():
    ap = argparse.ArgumentParser(description="check every #fragment link in the books")
    ap.add_argument("--root", default=None)
    ap.add_argument("--book", action="append", default=[])
    ap.add_argument("files", nargs="*")
    a = ap.parse_args()
    root = os.path.abspath(a.root or os.getcwd())
    pages = []
    for b in a.book or ([] if a.files else BOOKS):
        b = b.rstrip("/")
        for dp, dn, fn in os.walk(os.path.join(root, b)):
            dn.sort()
            for f in sorted(fn):
                if f.endswith(".md"):
                    pages.append(os.path.relpath(os.path.join(dp, f), root))
    if not a.book and not a.files and os.path.isfile(os.path.join(root, "README.md")):
        pages.append("README.md")
    for f in a.files:
        pages.append(os.path.relpath(os.path.abspath(f), root))
    gen = Generated(root)
    bad = Found()
    for rel in pages:
        if not os.path.isfile(os.path.join(root, rel)):
            bad.append("%s (no such page)" % rel)
            continue
        check_page(root, rel, gen, bad)
    for b in bad:
        print("BADANCHOR " + b)
    print("anchorcheck: %d pages, %d fragment links, %s"
          % (len(pages), bad.checked, "%d broken" % len(bad) if bad else "ok"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
