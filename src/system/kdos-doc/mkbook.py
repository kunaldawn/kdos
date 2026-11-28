#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   mkbook — the KDOS book on the image, and a manual page per KDOS command
#
#     mkbook.py DOCS DEST SRC
#     mkbook.py --html DOCS DEST [--site URL]
#
#   DOCS  a directory holding the book in kdos/ and its pictures in
#         screenshots/, and nothing else is read from it: the port's own
#         directory, whose `kdos` and `screenshots` links name exactly those
#         two, or the repository's docs/
#   DEST  the package root; with --html, the directory the book's
#         README.html lands in
#   SRC   the repository's src/, read for the manual pages programs ship
#         themselves
#   URL   the site the HTML book is published on, DEST being its book/: every
#         page gets a line linking the site's homepage and the book's
#         contents, and its canonical address under URL book/; a link into
#         docs/kfs goes to KDOS from Scratch at URL kfs/, the page kfsgen.py
#         renders it as, and a link to the repository's README.md to URL,
#         the homepage written from the same source, instead of the
#         Markdown on GitHub
#
# THREE OUTPUTS, ONE SOURCE. The book is rendered to HTML under
# /usr/share/doc/kdos with the tree's own layout and kdos.css from beside
# this file inside every page; kdos(7) says where it is and lists its
# chapters; and every
# `kdos-*` row of the command index, and the row of each command in OURS,
# becomes a manual page whose DESCRIPTION is the book section that row links
# to, each link in it to another page of the book printed as the `kdos-doc`
# command that opens that page. A manual page written by hand beside
# the book would be a second description going stale; this one is the book's
# own words. --html writes the first output alone, straight into DEST, which
# is how the published site is made. Every page is <html lang="en">.
#
# DOCS IS TWO LINKS IN THE PORT, NOT ONE TO docs/: the recipe hash follows
# every link in a port's directory, so a link to the whole of docs/ would
# rebuild this port for an edit to any other book kept there.
#
# A COMMAND WHOSE PROGRAM SHIPS ITS OWN PAGE IS LEFT TO IT: a `kdos-*.1` file
# anywhere under SRC names one, and two packages owning one path is a conflict
# kpkg refuses.
#
# NOTHING IS FETCHED and nothing runs but lowdown, with an argument vector.
# ---------------------------------

import os
import re
import subprocess
import sys
import time

GITHUB = "https://github.com/kunaldawn/kdos/blob/main/"
DOCROOT = "/usr/share/doc/kdos"

LINK_RE = re.compile(r'!?\[((?:[^\[\]]|\[[^\]]*\])*)\]\(([^()\s]+)(?:\s+"[^"]*")?\)')
ATTR_RE = re.compile(r'\b(href|src)="([^"]*)"')
SCHEME_RE = re.compile(r'^[A-Za-z][A-Za-z0-9+.-]*:')
FENCE_RE = re.compile(r'^\s*(```|~~~)')
HEAD_RE = re.compile(r'^(#{1,6})\s+(.*?)\s*#*\s*$')
CELL_RE = re.compile(r'(?<!\\)\|')
NAME_RE = re.compile(r'^`([a-z][a-z0-9-]*)`$')
# The commands this repository ships under a name without the kdos- prefix.
# A name is listed by hand rather than taken from every row: the command
# index also names upstream programs (ping, getent, init), whose own packages
# install their manual pages, and a second page at the same path is a file
# conflict kpkg refuses.
OURS = {"kpkg", "kpkgadd", "kpkgdel", "kpkgbuild", "kpkgdepends", "ksvc",
        "service", "kinstall", "xdg-terminal-exec"}
HTAG_RE = re.compile(r'<h([1-6])(?:\s+id="[^"]*")?>')
DOC_RE = re.compile(r'^\[[^\]]*\]\(([^)#]+)(#[^)]*)?\)$')
# What a NAME line must not leave to lowdown. The line is plain text taken
# out of a table cell, and lowdown reads it as markdown: `$XDG_RUNTIME_DIR`
# would lose its underscores to emphasis and `[ARGS]` could open a link.
MD_SPECIAL_RE = re.compile(r'([\\`*_{}\[\]<>()#!|&~^])')
# A NAME line is the whatis line apropos prints in whatever locale it runs
# in, a root console's C locale among them, where a typographic character
# prints as question marks; it is held to ASCII.
ASCII_OF = {"\u2026": "...", "\u2014": "--", "\u2013": "-", "\u2018": "'",
            "\u2019": "'", "\u201c": '"', "\u201d": '"', "\u2192": "->",
            "\u00b7": "-"}
# Manual pages are rendered without lowdown's smart typography: a page is
# read for what to type, and the filter turns `--json` into an en dash and
# straight quotes into curly ones.
MAN_ARGS = ["-s", "-tman", "--out-no-smarty"]
# apropos and whatis print the description makewhatis stored, and makewhatis
# keeps only the first 150 bytes of the NAME line's roff after the dash,
# every escape counted as written: lowdown writes " ^ ` ~ as a four-byte
# \(xx escape and a backslash as \e, every other character as its UTF-8. A
# longer line prints cut mid-word, so name_line ends one within the limit.
WHATIS_MAX = 150
ROFF_LEN = {'"': 4, "^": 4, "`": 4, "~": 4, "\\": 2}
CLAUSE_RE = re.compile(r"[,;:](?=\s)|\s(?=\()|\s(?=--\s)")


def lowdown(args, text):
    r = subprocess.run(["lowdown"] + args, input=text.encode(),
                       stdout=subprocess.PIPE, check=True)
    return r.stdout.decode()


def plain(md):
    """Inline markdown as plain text: links to their text, no code ticks."""
    md = LINK_RE.sub(lambda m: m.group(1), md)
    return md.replace("`", "").replace("**", "").replace("*", "")


def anchor(text):
    """The id GitHub and lowdown both give a heading."""
    s = plain(text).strip().lower()
    s = re.sub(r"[^\w\- ]", "", s)
    return s.replace(" ", "-")


def headings(lines):
    """(index, level, text, id) for every heading outside a code fence, with
    GitHub's -1, -2 suffix on a repeated id."""
    out, seen, fence = [], {}, False
    for i, line in enumerate(lines):
        if FENCE_RE.match(line):
            fence = not fence
            continue
        if fence:
            continue
        m = HEAD_RE.match(line)
        if not m:
            continue
        a = anchor(m.group(2))
        n = seen.get(a, 0)
        seen[a] = n + 1
        out.append((i, len(m.group(1)), m.group(2), a if not n else f"{a}-{n}"))
    return out


def title_of(path):
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = HEAD_RE.match(line)
            if m and len(m.group(1)) == 1:
                return plain(m.group(2))
    return os.path.splitext(os.path.basename(path))[0]


# ── the HTML book ─────────────────────────────────────────────────────────

def kfs_slug(s):
    """kfsgen.py's slug()."""
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", s).strip("-")


def kfs_url(site, rel, frag):
    """A docs/kfs page, REL under docs/kfs, as KDOS from Scratch is published
    at SITE kfs/: the file kfsgen.py's page id names (its Page: a README is
    `index`, a part's intro.md the part, any other page `<part>-<stem>`), and
    a heading's id within it, `<page id>--<GitHub's id>`. mksite.py checks
    every such link against the rendered book before the site is deployed."""
    d, f = os.path.split(rel)
    stem = f[:-3]
    if not d:
        pid = "index" if stem == "README" else kfs_slug(stem)
    elif stem == "intro":
        pid = kfs_slug(d)
    else:
        pid = kfs_slug(d + "-" + stem)
    out = site + "kfs/" + ("" if pid == "index" else pid + ".html")
    return out + (f"#{pid}--{frag}" if frag else "")


def rewrite_target(value, src_rel, docs, site=None):
    """A link as the installed book needs it. `src_rel` is the page's path
    under DOCS (`kdos/04-programs/x.md`); the answer is relative to the
    installed page. A chapter link becomes its .html, a picture moves to
    screenshots/ under the book, and anything else in the repository becomes
    a link to the repository, since it is not on the image; with SITE, a
    page of docs/kfs becomes its page of the site's KDOS from Scratch, and
    the repository's README.md the site's homepage, which is written from
    the same source."""
    if not value or value.startswith("#") or value.startswith("/") \
            or SCHEME_RE.match(value):
        return value
    path, _, frag = value.partition("#")
    target = os.path.normpath(os.path.join(os.path.dirname(src_rel), path))
    if site and target == os.path.join(os.pardir, "README.md"):
        return site + (("#" + frag) if frag else "")
    if site and target == "kfs":
        return kfs_url(site, "README.md", frag)
    if site and target.startswith("kfs/") and target.endswith(".md"):
        return kfs_url(site, target[4:], frag)
    if target == "kdos" or target.startswith("kdos/"):
        out = target[5:] if target != "kdos" else ""
        if os.path.isdir(os.path.join(docs, target)):
            out = os.path.join(out, "README.html")
        elif out.endswith(".md"):
            out = out[:-3] + ".html"
    elif target.startswith("screenshots/"):
        out = target
    else:
        return GITHUB + os.path.normpath(os.path.join("docs", target)) + \
            (("#" + frag) if frag else "")
    here = os.path.dirname(src_rel[5:])
    out = os.path.relpath(out, here or ".")
    return out + (("#" + frag) if frag else "")


def github_ids(html, text, rel):
    """Every heading's id as GitHub writes it. The book's links are checked
    against GitHub's ids, and lowdown makes its own (it keeps a `.`, folds a
    `--`), so with lowdown's ids every link to a heading whose text carries
    punctuation lands at the top of its page. The headings are matched in
    order, which holds only while lowdown and headings() agree on what a
    heading is, so a page where they disagree stops the build."""
    want = [h[3] for h in headings(text.split("\n"))]
    tags = list(HTAG_RE.finditer(html))
    if len(tags) != len(want):
        sys.exit(f"mkbook: {rel}: lowdown rendered {len(tags)} headings, "
                 f"the page has {len(want)}")
    out, at = [], 0
    for m, a in zip(tags, want):
        out.append(html[at:m.start()])
        out.append(f'<h{m.group(1)} id="{a}">')
        at = m.end()
    out.append(html[at:])
    return "".join(out)


def copy_file(src, dst):
    with open(src, "rb") as f:
        data = f.read()
    with open(dst, "wb") as f:
        f.write(data)


HTML_RE = re.compile(r"<html[^>]*>")
BODY_RE = re.compile(r"<body[^>]*>")


def site_head(site, out_rel):
    """The canonical address of a page of the book at SITE book/; README.html
    is the book's index, so its address is the directory."""
    page = "" if out_rel == "README.html" else out_rel
    return f'<link rel="canonical" href="{site}book/{page}">\n'


def site_line(out_rel):
    """The line above a page of the published book: the site's homepage, one
    level above the book, and the book's contents."""
    up = "../" * out_rel.count("/")
    return (f'<nav class="site-line" aria-label="kdos.dev">'
            f'<a href="{up}../">kdos.dev</a> · '
            f'<a href="{up}README.html">The KDOS book</a></nav>\n')


def render_book(docs, root, site=None):
    book = os.path.join(docs, "kdos")
    if not os.path.isfile(os.path.join(book, "README.md")):
        sys.exit(f"mkbook: {docs} holds no kdos/README.md")
    # THE STYLESHEET IS INSIDE EACH PAGE. KHelpCenter shows a help:/kdos/…
    # page through KIO's help worker and answers every request of that page,
    # its stylesheet included, as text/html, and the browser engine refuses
    # a linked stylesheet of that type; a <style> element is part of the page
    # and holds under file:, help: and the published site alike.
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "kdos.css"), encoding="utf-8") as f:
        style = "<style>\n" + f.read() + "</style>\n"
    chapters = []
    for d, dirs, files in os.walk(book):
        dirs.sort()
        for name in sorted(files):
            if not name.endswith(".md"):
                continue
            src = os.path.join(d, name)
            rel = os.path.relpath(src, docs)
            out_rel = rel[5:-3] + ".html"
            title = title_of(src)
            with open(src, encoding="utf-8") as f:
                text = f.read()
            html = lowdown(["-s", "-thtml", "--html-no-skiphtml",
                            "--html-no-escapehtml",
                            "-M", "title=" + title], text)
            if "</head>" not in html or not HTML_RE.search(html) \
                    or not BODY_RE.search(html):
                sys.exit(f"mkbook: {rel}: lowdown wrote no <html>, </head> or <body>")
            # The language is set here, not by lowdown's metadata, which
            # not every lowdown the published site is made with reads.
            html = HTML_RE.sub('<html lang="en">', html, 1)
            head = style + (site_head(site, out_rel) if site else "")
            html = html.replace("</head>", head + "</head>", 1)
            html = github_ids(html, text, rel)
            html = ATTR_RE.sub(
                lambda m: '%s="%s"' % (m.group(1),
                                       rewrite_target(m.group(2), rel, docs, site)),
                html)
            if site:
                b = BODY_RE.search(html)
                html = html[:b.end()] + "\n" + site_line(out_rel) + html[b.end():]
            out = os.path.join(root, out_rel)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with open(out, "w", encoding="utf-8") as f:
                f.write(html)
            chapters.append((out_rel, title))
    shots = os.path.join(docs, "screenshots")
    if os.path.isdir(shots):
        os.makedirs(os.path.join(root, "screenshots"), exist_ok=True)
        for name in sorted(os.listdir(shots)):
            copy_file(os.path.join(shots, name),
                      os.path.join(root, "screenshots", name))
    os.symlink("README.html", os.path.join(root, "index.html"))
    return chapters


# ── the manual pages ──────────────────────────────────────────────────────

def doc_ref(target, path, book):
    """The `kdos-doc` argument that opens a link's target, when the target is
    a page of the book: the page's path under BOOK without `.md`, and its
    fragment when it names one. None for anything else, a same-page fragment,
    a file outside the book or a page that is not markdown."""
    page, _, frag = target.partition("#")
    if not page.endswith(".md"):
        return None
    dest = os.path.normpath(os.path.join(os.path.dirname(path), page))
    rel = os.path.relpath(dest, book)
    if rel.startswith(".."):
        return None
    return rel[:-3].replace(os.sep, "/") + ("#" + frag if frag else "")


def man_link(m, path, book):
    """A link as a manual page prints it. A man reader has nothing to click
    and no book page open, so a link to another page of the book becomes its
    text and the `kdos-doc` command that opens that page; a URL with a
    scheme stays a link, which lowdown prints with its address; any other
    target, a same-page fragment or a file outside the book, leaves its text
    alone."""
    text, target = m.group(1), m.group(2)
    if SCHEME_RE.match(target):
        return m.group(0)
    ref = doc_ref(target, path, book)
    return f"{text} (kdos-doc {ref})" if ref else text


def section_text(path, frag, book=None):
    """The markdown of one book section: from the heading the fragment names
    to the next heading at its level or above, or the whole page after its
    title when there is no fragment. Headings inside become the manual's
    subsections, pictures are dropped and links are as man_link prints them.
    BOOK is the book's directory, the kdos/ that holds the page's part; it
    defaults to the directory above the page's own."""
    if book is None:
        book = os.path.dirname(os.path.dirname(os.path.abspath(path)))
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    heads = headings(lines)
    if frag:
        hit = [h for h in heads if h[3] == frag]
        if not hit:
            return None
        start, level = hit[0][0], hit[0][1]
    else:
        top = [h for h in heads if h[1] == 1]
        start, level = (top[0][0], 1) if top else (-1, 0)
    end = len(lines)
    for i, lv, _, _ in heads:
        if i > start and lv <= level:
            end = i
            break
    body, fence = [], False
    for line in lines[start + 1:end]:
        if FENCE_RE.match(line):
            fence = not fence
        m = None if fence else HEAD_RE.match(line)
        if m:
            line = "## " + m.group(2)
        body.append(line)
    text = "\n".join(body)
    text = re.sub(r'!\[[^\]]*\]\([^)]*\)', "", text)
    text = LINK_RE.sub(lambda m: man_link(m, os.path.abspath(path), book),
                       text)
    return text.strip() + "\n"


def own_pages(src):
    names = set()
    for d, _, files in os.walk(src):
        for name in files:
            if re.match(r"^kdos-[a-z0-9-]+\.[1-8]$", name):
                names.add(name.rsplit(".", 1)[0])
    return names


def commands(docs):
    """(name, description, page, fragment, section) for every kdos-* row of
    the command index, and every row of a name in OURS, that links a book
    section, first row per name, up to the host-only tools. A row's first
    cell is the name, its last the link, and the one before the link what the
    command does, whatever columns (an install directory, a port) sit
    between. A root daemon, and a command of OURS installed in /usr/sbin, is
    section 8, the rest section 1."""
    index = os.path.join(docs, "kdos", "06-reference", "command-index.md")
    out, seen, part = [], set(), ""
    with open(index, encoding="utf-8") as f:
        for line in f:
            if line.startswith("## "):
                part = line[3:].strip()
                if part.startswith("Host-only"):
                    break
                continue
            if not line.startswith("|"):
                continue
            cells = [c.strip() for c in CELL_RE.split(line.strip())[1:-1]]
            if len(cells) < 3:
                continue
            n, d = NAME_RE.match(cells[0]), DOC_RE.match(cells[-1])
            if not n or not d or n.group(1) in seen:
                continue
            name = n.group(1)
            if not name.startswith("kdos-") and name not in OURS:
                continue
            seen.add(name)
            page = os.path.normpath(os.path.join(
                os.path.dirname(index), d.group(1)))
            frag = (d.group(2) or "#")[1:]
            sbin = name in OURS and "`/usr/sbin`" in cells
            sec = ("8" if (part == "Daemons" and name.endswith("d")) or sbin
                   else "1")
            desc = plain(cells[-2]).replace("\\|", "|")
            out.append((name, desc, page, frag, sec))
    return out


def roff_len(text):
    """The bytes lowdown writes for an ASCII NAME description."""
    return sum(ROFF_LEN.get(c, len(c.encode())) for c in text)


def whatis(desc):
    """An ASCII description that makewhatis stores whole: unchanged when it
    fits WHATIS_MAX, else ended before the last clause separator that fits,
    or at the last space when no clause does, with no bracket left open; a
    description with neither is cut at the limit."""
    if roff_len(desc) <= WHATIS_MAX:
        return desc
    fit = 0
    while roff_len(desc[:fit + 1]) <= WHATIS_MAX:
        fit += 1
    ends = [m.start() for m in CLAUSE_RE.finditer(desc) if m.start() <= fit]
    cut = ends[-1] if ends else desc.rfind(" ", 0, fit + 1)
    head = desc[:max(cut, 0)]
    for o, c in ("()", "[]"):
        while head.count(o) > head.count(c):
            head = head[:head.rfind(o)]
    return head.rstrip(" ,;:-") or desc[:fit]


def name_line(desc):
    """A row's description as the markdown of a NAME line: ASCII, short
    enough for apropos to print whole, and every character lowdown would
    read as markup escaped."""
    desc = whatis("".join(ASCII_OF.get(c, c) for c in desc))
    return MD_SPECIAL_RE.sub(r"\\\1", desc)


# A section of the book says "this chapter" of itself; under man(1) it is a
# page, and a reader of kinstall(1) has no chapter in front of them.
CHAPTER_RE = re.compile(r'\b([Tt])his chapter\b')


def man_body(body):
    """A book section's markdown as a manual page's DESCRIPTION."""
    return CHAPTER_RE.sub(lambda m: m.group(1) + "his page", body)


def man_page(name, sec, desc, body, chapter, date):
    md = (f"# NAME\n\n{name} \\- {name_line(desc)}\n\n"
          f"# DESCRIPTION\n\n{man_body(body)}\n"
          f"# SEE ALSO\n\n**kdos**(7), and the chapter this page is taken "
          f"from, *{DOCROOT}/{chapter}*.\n")
    return lowdown(MAN_ARGS + ["-M", "title=" + name.upper(),
                               "-M", "section=" + sec,
                               "-M", "date=" + date,
                               "-M", "source=KDOS"], md)


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def main():
    usage = "usage: mkbook.py DOCS DEST SRC | mkbook.py --html DOCS DEST [--site URL]"
    if len(sys.argv) in (4, 6) and sys.argv[1] == "--html":
        site = None
        if len(sys.argv) == 6:
            if sys.argv[4] != "--site" or not re.match(r"^https?://.*/$", sys.argv[5]):
                sys.exit(usage + "\n  --site takes an http(s) URL ending in /")
            site = sys.argv[5]
        chapters = render_book(sys.argv[2], sys.argv[3], site)
        print(f"mkbook: {len(chapters)} chapters")
        return
    if len(sys.argv) != 4 or sys.argv[1].startswith("-"):
        sys.exit(usage)
    docs, dest, src = sys.argv[1:]
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
    date = time.strftime("%Y-%m-%d", time.gmtime(epoch))

    chapters = render_book(docs, os.path.join(dest, DOCROOT.lstrip("/")))

    skip = own_pages(src)
    made, missing = [], []
    for name, desc, page, frag, sec in commands(docs):
        if name in skip:
            made.append((name, sec))
            continue
        body = section_text(page, frag, os.path.join(docs, "kdos"))
        if body is None:
            missing.append(f"{name}: no section #{frag} in "
                           f"{os.path.relpath(page, docs)}")
            continue
        chapter = os.path.relpath(page, os.path.join(docs, "kdos"))
        chapter = chapter[:-3] + ".html"
        write(os.path.join(dest, f"usr/share/man/man{sec}/{name}.{sec}"),
              man_page(name, sec, desc, body, chapter, date))
        made.append((name, sec))
    if missing:
        sys.exit("mkbook: the command index links sections that are not "
                 "there:\n  " + "\n  ".join(missing))

    toc = "\n".join(f"- {t}: *{DOCROOT}/{p}*" for p, t in sorted(chapters))
    cmds = ", ".join(f"**{n}**({s})" for n, s in sorted(made))
    md = ("# NAME\n\nkdos \\- the KDOS book, on this machine\n\n"
          "# DESCRIPTION\n\n"
          f"The whole KDOS book is installed as HTML under *{DOCROOT}*; open "
          f"*{DOCROOT}/index.html* in a browser for its title page, reading "
          "paths and table of contents. Every **kdos-**\\* command, and "
          "**kpkg**, **ksvc**, **service**, **kinstall** and "
          "**xdg-terminal-exec**, has a manual page taken from the section "
          "of the book that documents it.\n\n## Chapters\n\n" + toc + "\n\n"
          "# SEE ALSO\n\n" + cmds + "\n")
    write(os.path.join(dest, "usr/share/man/man7/kdos.7"),
          lowdown(MAN_ARGS + ["-M", "title=KDOS", "-M", "section=7",
                              "-M", "date=" + date, "-M", "source=KDOS"],
                  md))
    print(f"mkbook: {len(chapters)} chapters, {len(made)} command pages")


if __name__ == "__main__":
    main()
