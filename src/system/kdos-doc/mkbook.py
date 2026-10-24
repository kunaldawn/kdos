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
#     mkbook.py --html DOCS DEST
#
#   DOCS  a directory holding the book in kdos/ and its pictures in
#         screenshots/, and nothing else is read from it: the port's own
#         directory, whose `kdos` and `screenshots` links name exactly those
#         two, or the repository's docs/
#   DEST  the package root; with --html, the directory the book's
#         README.html lands in
#   SRC   the repository's src/, read for the manual pages programs ship
#         themselves
#
# THREE OUTPUTS, ONE SOURCE. The book is rendered to HTML under
# /usr/share/doc/kdos with the tree's own layout and kdos.css from beside
# this file; kdos(7) says where it is and lists its chapters; and every
# `kdos-*` row of the command index, and the row of each command in OURS,
# becomes a manual page whose DESCRIPTION is the book section that row links
# to. A manual page written by hand beside
# the book would be a second description going stale; this one is the book's
# own words. --html writes the first output alone, straight into DEST, which
# is how the published site is made.
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

def rewrite_target(value, src_rel, docs):
    """A link as the installed book needs it. `src_rel` is the page's path
    under DOCS (`kdos/04-programs/x.md`); the answer is relative to the
    installed page. A chapter link becomes its .html, a picture moves to
    screenshots/ under the book, and anything else in the repository becomes
    a link to the repository, since it is not on the image."""
    if not value or value.startswith("#") or value.startswith("/") \
            or SCHEME_RE.match(value):
        return value
    path, _, frag = value.partition("#")
    target = os.path.normpath(os.path.join(os.path.dirname(src_rel), path))
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


def render_book(docs, root):
    book = os.path.join(docs, "kdos")
    if not os.path.isfile(os.path.join(book, "README.md")):
        sys.exit(f"mkbook: {docs} holds no kdos/README.md")
    chapters = []
    for d, dirs, files in os.walk(book):
        dirs.sort()
        for name in sorted(files):
            if not name.endswith(".md"):
                continue
            src = os.path.join(d, name)
            rel = os.path.relpath(src, docs)
            out_rel = rel[5:-3] + ".html"
            depth = out_rel.count("/")
            title = title_of(src)
            with open(src, encoding="utf-8") as f:
                text = f.read()
            html = lowdown(["-s", "-thtml", "--html-no-skiphtml",
                            "--html-no-escapehtml",
                            "-M", "title=" + title,
                            "-M", "css=" + "../" * depth + "kdos.css"], text)
            html = github_ids(html, text, rel)
            html = ATTR_RE.sub(
                lambda m: '%s="%s"' % (m.group(1),
                                       rewrite_target(m.group(2), rel, docs)),
                html)
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
    copy_file(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "kdos.css"), os.path.join(root, "kdos.css"))
    os.symlink("README.html", os.path.join(root, "index.html"))
    return chapters


# ── the manual pages ──────────────────────────────────────────────────────

def section_text(path, frag):
    """The markdown of one book section: from the heading the fragment names
    to the next heading at its level or above, or the whole page after its
    title when there is no fragment. Headings inside become the manual's
    subsections."""
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
    text = LINK_RE.sub(lambda m: m.group(1)
                       if not SCHEME_RE.match(m.group(2)) else m.group(0),
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


def man_page(name, sec, desc, body, chapter, date):
    md = (f"# NAME\n\n{name} \\- {desc}\n\n"
          f"# DESCRIPTION\n\n{body}\n"
          f"# SEE ALSO\n\n**kdos**(7), and the chapter this page is taken "
          f"from, *{DOCROOT}/{chapter}*.\n")
    return lowdown(["-s", "-tman",
                    "-M", "title=" + name.upper(),
                    "-M", "section=" + sec,
                    "-M", "date=" + date,
                    "-M", "source=KDOS"], md)


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def main():
    usage = "usage: mkbook.py DOCS DEST SRC | mkbook.py --html DOCS DEST"
    if len(sys.argv) == 4 and sys.argv[1] == "--html":
        chapters = render_book(sys.argv[2], sys.argv[3])
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
        body = section_text(page, frag)
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
          lowdown(["-s", "-tman", "-M", "title=KDOS", "-M", "section=7",
                   "-M", "date=" + date, "-M", "source=KDOS"], md))
    print(f"mkbook: {len(chapters)} chapters, {len(made)} command pages")


if __name__ == "__main__":
    main()
