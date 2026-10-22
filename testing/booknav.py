#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   booknav — the navigation footer of every chapter, and the contents of
#   KDOS from Scratch
# ---------------------------------
#
#     booknav.py [--root DIR] [--book docs/kdos|docs/kfs]... [--write]
#
# With no --book it handles both books. Without --write it changes nothing
# and exits 1 when any footer, or the docs/kfs contents, differs from what it
# would write, naming each page; --write rewrites them.
#
# THE FOOTER is everything after the page's `<!-- book-nav -->` line, which
# is the last thing on the page:
#
#     <!-- book-nav -->
#     ---
#
#     *Part II — Using KDOS, chapter 10.* Previous: [9. Theming](theming.md) · [Contents](../README.md) · Next: [11. Accessibility](accessibility.md)
#
# Previous is left out on the first page of a book and Next on the last. A
# page with no marker gets one on --write, after a blank line. Every link is
# relative to the page.
#
# docs/kdos IS READ, NEVER DERIVED: its README.md's `## Contents` is the
# authority on the order. Each `### Part <N> — <name>` heading opens a part
# and each `<n>. [<title>](<page>)` line under it is a chapter; a footer
# names the part, the chapter's number, and its neighbours by number and by
# the title the contents gives them. A number out of sequence, or a listed
# page that does not exist, fails the check, because every footer after it
# would be wrong.
#
# docs/kfs IS DERIVED, and this writes its contents. The order is the one
# script/book/kfsgen.py renders: the part directories in name order, each
# part's intro.md first, its pages in the order its `kfs:order` directive
# gives and then by name. The chapter numbers are the rendered book's,
# which counts the generated chapters (one per shelf of a package phase,
# and the chapter of ports no phase builds) as well as the hand-written
# ones, so this runs kfsgen into a temporary directory and reads its
# units.json. A recipe that gives a phase its first port on a new shelf
# therefore renumbers every later chapter, and the check fails until
# --write is run. A page named after a shelf in a package part is that
# shelf chapter's narrative: it takes the shelf chapter's number and place,
# and one that matches no shelf chapter is refused, because kfsgen renders
# it nowhere.
#
# The docs/kfs contents is the text between `<!-- book-contents -->` and
# `<!-- /book-contents -->` in its README.md: a `### [<part>](<intro>)`
# heading per part, its hand-written chapters as a numbered list (a bulleted
# one for the unnumbered front matter) under a `#### <label>` heading where
# the part's kfs:order directive groups them, and one line naming the part's
# generated chapters, each shelf linked to its narrative page when it has
# one. In a part that groups its pages, a page the directive does not name
# is a problem, since it would land under the last group's heading.

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

MARK = "<!-- book-nav -->"
C_OPEN = "<!-- book-contents -->"
C_CLOSE = "<!-- /book-contents -->"
BOOKS = ["docs/kdos", "docs/kfs"]
PART_RE = re.compile(r"^###\s+Part\s+([IVXLC]+)\s+—\s+(.+?)\s*$")
ITEM_RE = re.compile(r"^(\d+)\.\s+\[([^\]]+)\]\(([^)\s]+)\)")


class Nav:
    """One page in reading order: its path from the book, the label the
    footer opens with, and the text a neighbour's link to it shows."""

    def __init__(self, rel, label, link_text):
        self.rel = rel
        self.label = label
        self.link_text = link_text


def footer(book, seq, i):
    here = os.path.dirname(seq[i].rel)

    def rel(to):
        return os.path.relpath(to, here or ".")

    bits = []
    if i > 0:
        bits.append("Previous: [%s](%s)" % (seq[i - 1].link_text, rel(seq[i - 1].rel)))
    bits.append("[Contents](%s)" % rel("README.md"))
    if i + 1 < len(seq):
        bits.append("Next: [%s](%s)" % (seq[i + 1].link_text, rel(seq[i + 1].rel)))
    return "%s\n---\n\n*%s.* %s\n" % (MARK, seq[i].label, " · ".join(bits))


def kdos_order(bookdir, problems):
    with open(os.path.join(bookdir, "README.md"), encoding="utf-8") as f:
        lines = f.read().split("\n")
    seq, part, inside, want = [], None, False, 1
    for line in lines:
        if line.startswith("## "):
            inside = line.strip() == "## Contents"
            continue
        if not inside:
            continue
        m = PART_RE.match(line)
        if m:
            part = "Part %s — %s" % (m.group(1), m.group(2))
            continue
        m = ITEM_RE.match(line)
        if not m:
            continue
        n, title, target = int(m.group(1)), m.group(2), m.group(3)
        if part is None:
            problems.append("README.md: chapter %d is listed before any part" % n)
        if n != want:
            problems.append("README.md: chapter %d is listed where %d belongs" % (n, want))
        want = n + 1
        rel = os.path.normpath(target.partition("#")[0])
        if not os.path.isfile(os.path.join(bookdir, rel)):
            problems.append("README.md: chapter %d names %s, which does not exist" % (n, target))
        seq.append(Nav(rel, "%s, chapter %d" % (part, n), "%d. %s" % (n, title)))
    if not seq:
        problems.append("README.md: no chapters under ## Contents")
    return seq


def load_kfsgen(root):
    import importlib.util
    # No __pycache__ beside kfsgen.py: script/book ships inside kdos-kfs,
    # and the checkout should hold only the files it commits. Both recipe
    # hashes skip a cache that does get written.
    sys.dont_write_bytecode = True
    path = os.path.join(root, "script/book/kfsgen.py")
    spec = importlib.util.spec_from_file_location("kfsgen", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def kfs_units(root):
    with tempfile.TemporaryDirectory(prefix="booknav.") as tmp:
        out = os.path.join(tmp, "src")
        r = subprocess.run([sys.executable, os.path.join(root, "script/book/kfsgen.py"),
                            "--root", root, "--out", out],
                           capture_output=True, text=True)
        try:
            with open(os.path.join(out, "units.json"), encoding="utf-8") as f:
                return json.load(f)["units"]
        except OSError:
            sys.exit("booknav: kfsgen wrote no units.json:\n" + r.stderr[-2000:])


def kfs_order(root, bookdir, problems):
    """(reading order, contents text)."""
    kg = load_kfsgen(root)
    units = kfs_units(root)
    pages = {}
    for d in [""] + kg.part_dirs(bookdir):
        for f in sorted(os.listdir(os.path.join(bookdir, d))):
            if f.endswith(".md"):
                pg = kg.Page(bookdir, os.path.join(bookdir, d, f))
                pages[pg.id] = pg
    shelf_pages, group_of = {}, {}
    for d in kg.part_dirs(bookdir):
        intro = pages.get(kg.slug(d))
        groups = kg.order_groups(intro)
        for label, stem in groups:
            if label:
                group_of[kg.slug(d + "-" + stem)] = label
        # A page the order does not name follows every named one, and in a
        # part with `[Label]` groups it would sit under the last heading.
        if any(label for label, _ in groups):
            named = {stem for _, stem in groups}
            for f in sorted(os.listdir(os.path.join(bookdir, d))):
                if f.endswith(".md") and f != "intro.md" and f[:-3] not in named:
                    problems.append("%s/%s: %s/intro.md groups its pages with [Label]s, and "
                                    "its kfs:order does not name this one" % (d, f, d))
        ph = intro.directives.get("phase", "") if intro else ""
        for f in sorted(os.listdir(os.path.join(bookdir, d))):
            if f.endswith(".md") and f != "intro.md":
                stem = f[:-3]
                pg = pages.get(kg.slug(d + "-" + stem))
                if ph and any(u["id"] == "shelf-%s-%s" % (ph, stem) for u in units):
                    shelf_pages["shelf-%s-%s" % (ph, stem)] = pg

    seq, placed, contents = [], set(), []
    part_label, gen, group = None, [], None

    def flush_generated():
        if not gen:
            return
        nums = [u["number"] for u in gen]
        span = ("Chapter %s" % nums[0]) if len(nums) == 1 else \
            ("Chapters %s–%s" % (nums[0], nums[-1]))
        names = []
        for u in gen:
            if u["id"] == "shelf-unphased":
                t = re.sub(r"^\d+\.\s*", "", u["title"])
            else:
                # shelf-<phase>-<shelf>: a phase name has no '-'.
                t = "`%s`" % u["id"].split("-", 2)[2]
            pg = shelf_pages.get(u["id"])
            names.append("[%s](%s)" % (t, pg.rel) if pg else t)
        contents.append("")
        contents.append("%s, %s: %s."
                        % (span, "the generated package chapters, one per shelf" if len(gen) > 1
                           else "the generated package chapter", ", ".join(names)))
        gen.clear()

    for u in units:
        if u["level"] == 1:
            flush_generated()
            pg = pages.get(u["id"])
            if pg is None:
                continue
            if u["number"]:
                # The rendered book's own form, "Part I. Preparing".
                part_label = "Part %s. %s" % (u["number"], pg.title)
            else:
                part_label = pg.title
            seq.append(Nav(pg.rel, part_label, part_label))
            placed.add(pg.id)
            group = None
            contents.append("")
            contents.append("### [%s](%s)" % (part_label, pg.rel))
            contents.append("")
            continue
        if u["level"] != 2:
            continue
        pg = pages.get(u["id"]) if u["kind"] == "chapter" else None
        if pg is not None:
            flush_generated()
            if group_of.get(pg.id, group) != group:
                group = group_of[pg.id]
                contents.extend(["", "#### %s" % group, ""])
            if u["number"]:
                seq.append(Nav(pg.rel, "%s, chapter %s" % (part_label, u["number"]),
                               "%s. %s" % (u["number"], pg.title)))
                contents.append("%s. [%s](%s)" % (u["number"], pg.title, pg.rel))
            else:
                seq.append(Nav(pg.rel, part_label, pg.title))
                contents.append("- [%s](%s)" % (pg.title, pg.rel))
            placed.add(pg.id)
            continue
        gen.append(u)
        pg = shelf_pages.get(u["id"])
        if pg is not None:
            seq.append(Nav(pg.rel, "%s, chapter %s" % (part_label, u["number"]),
                           "%s. %s" % (u["number"], pg.title)))
            placed.add(pg.id)
    flush_generated()
    for pid, pg in sorted(pages.items()):
        if pid not in placed and pg.rel != "README.md":
            problems.append("%s: kfsgen renders it nowhere (a page named after a shelf "
                            "needs that shelf to have a port in the part's phase)" % pg.rel)
    while contents and not contents[0]:
        contents.pop(0)
    # Collapse the blank line a list item would otherwise get after a heading.
    text = "\n".join(contents)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return seq, text + "\n"


def apply_contents(path, body, write, problems):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    a, b = text.find(C_OPEN), text.find(C_CLOSE)
    if a < 0 or b < a:
        problems.append("%s: no %s … %s block for the contents" % (path, C_OPEN, C_CLOSE))
        return False
    new = text[:a] + C_OPEN + "\n\n" + body + "\n" + text[b:]
    if new == text:
        return False
    if write:
        with open(path, "w", encoding="utf-8") as f:
            f.write(new)
    return True


def apply_footer(path, want, write):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    k = text.find("\n" + MARK + "\n")
    if k >= 0:
        new = text[:k + 1] + want
    elif text.startswith(MARK + "\n"):
        new = want
    else:
        new = text.rstrip("\n") + "\n\n" + want
    if new == text:
        return False
    if write:
        with open(path, "w", encoding="utf-8") as f:
            f.write(new)
    return True


def main():
    ap = argparse.ArgumentParser(description="write or check the books' navigation")
    ap.add_argument("--root", default=None)
    ap.add_argument("--book", action="append", default=[])
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    root = os.path.abspath(a.root or os.getcwd())
    rc = 0
    for book in a.book or BOOKS:
        book = book.rstrip("/")
        bookdir = os.path.join(root, book)
        problems, stale = [], []
        if not os.path.isfile(os.path.join(bookdir, "README.md")):
            print("booknav: %s has no README.md" % book)
            rc = 1
            continue
        if book.endswith("kfs"):
            seq, body = kfs_order(root, bookdir, problems)
            if apply_contents(os.path.join(bookdir, "README.md"), body, a.write, problems):
                stale.append("README.md (contents)")
        else:
            seq = kdos_order(bookdir, problems)
        seen = set()
        for i, nav in enumerate(seq):
            if nav.rel in seen:
                problems.append("%s is in the reading order twice" % nav.rel)
                continue
            seen.add(nav.rel)
            path = os.path.join(bookdir, nav.rel)
            if not os.path.isfile(path):
                continue
            if apply_footer(path, footer(book, seq, i), a.write):
                stale.append(nav.rel)
        for p in problems:
            print("NAV       %s/%s" % (book, p))
        for s in stale:
            print("%s %s/%s" % ("WROTE    " if a.write else "STALE    ", book, s))
        if problems or (stale and not a.write):
            rc = 1
        tail = []
        if problems:
            tail.append("%d problems" % len(problems))
        if stale:
            tail.append("%d %s" % (len(stale), "rewritten" if a.write
                                   else "stale — run testing/booknav.py --write"))
        print("booknav: %s, %d pages, %s" % (book, len(seq), ", ".join(tail) or "ok"))
    return rc


if __name__ == "__main__":
    sys.exit(main())
