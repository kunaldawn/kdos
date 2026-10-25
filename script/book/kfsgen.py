#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   kfsgen — the Markdown source of KDOS from Scratch, from the tree
#
#     kfsgen.py [--root DIR] [--data DIR] [--out DIR]
#
#   --root  the repository (default: the current directory): ports/core,
#           src/, ports/shelves, ports/sources.idx, script/phases and
#           docs/kfs are read from it, and nothing under it is written
#   --data  the committed snapshot (default: <root>/docs/kfs/data), which
#           make_data.py writes; every file in it is optional
#   --out   where the source tree goes (default: <root>/build/kfs/src),
#           emptied first
#
# ONE BOOK, TWO HALVES. The hand-written pages under docs/kfs are the
# narrative; every package page is generated here, on every render, from the
# recipe and the phase lists, so no page can describe a recipe that is not
# the one in the tree. Nothing generated is committed.
#
# THE OUTPUT is book.md, every unit of the book in reading order, which
# kfsrender.py hands to pandoc once for HTML and once for typst; units.json,
# the unit list the HTML half splits book.md's output by and builds its
# navigation from; and one .md file per unit under parts/, the same text cut
# at the unit markers, for a reader who wants a single page's source. A unit
# is a part, a chapter or a package. Its marker is a raw HTML comment
# `<!--kfs-unit ID-->`, which the typst writer drops.
#
# RECIPES ARE PARSED, NEVER SOURCED, by the rules of kdos-kpkg's decl.c: a
# value is expanded as it is read, `$var`, `${var}` and the seven parameter
# forms, a helper may read an earlier one, and the repeatable keys (source,
# sha256, depends, pypackages) accumulate. A change to decl.c's rules is a
# change here, or a page shows a URL kpkg would not fetch.
#
# BUILD ORDER is resolved the way kpkgdepends resolves it and
# testing/phaseclosure.py checks it: phases in sorted order, each list depth
# first over `depends =`, a port an earlier phase installed neither emitted
# nor walked. A port's page sits in the phase that installs it, in its shelf's
# chapter, at its place in that order.
#
# EVERY STRING FROM A RECIPE IS ESCAPED before it reaches Markdown. A
# description is prose to kpkg and syntax to pandoc: `zng_`, `AX_*`, `<b>`,
# `@` and `$` are emphasis, raw HTML, a citation and maths to the reader, and
# an unclosed delimiter to typst. Backquoted spans in a phase-list comment are
# kept as code; everything else is escaped character by character.
#
# DIRECTIVES in a hand-written page, outside code fences:
#
#   <!-- kfs:include PATH [lines=A-B pin=HASH lang=NAME] -->   PATH from
#                     the repository root, as a fenced block; a whole file loses
#                     its KDOS banner. lang= names the block's language where
#                     the file's extension would name the wrong one: the
#                     lines of a shell block inside a .md page are shell, not
#                     Markdown. A range carries the pin of the lines
#                     it was written against (range_pin): lines added or
#                     removed above it would otherwise show the reader other
#                     lines than the prose explains, with nothing to say so.
#                     A range outside the file, or a pin that does not match
#                     its lines, is a warning here, and
#                     testing/kfscheck.py --repin moves a range whose lines
#                     are only elsewhere in the file
#   <!-- kfs:phase NAME -->    in a part's intro.md: the part is that phase's
#   <!-- kfs:order A B … -->   in a part's intro.md: the page order; pages it
#                              does not name follow, sorted. A `[Label]`
#                              between names heads the pages after it in
#                              README.md's contents (order_groups); the
#                              rendered book ignores it
#   <!-- kfs:unphased -->      in a part's intro.md: the chapter of ports no
#                              phase builds goes at the end of this part
#   <!-- kfs:sbu -->           in any page: the snapshot's stamp (the
#                              checkout's commit when it was gathered, the
#                              date of its newest installed package, the
#                              seconds one SBU took and the machine that
#                              measured it), how many figures share that
#                              machine and which do not, the package phases
#                              with a measured port, then one
#                              table per such phase of every port with a
#                              measured time, in build order, a figure from
#                              another machine or naming none marked `*`
#                              when the SBU names one. A port the
#                              snapshot has `-` for is left out, and the
#                              package phases with no measured port are
#                              named in one closing line
#   <!-- kfs:licences -->      in any page: every SPDX licence id the
#                              recipes' `license =` expressions name, with
#                              each port that names it, most-used first,
#                              then the `WITH` exceptions the same way. An
#                              expression is split into its ids, so a port
#                              under `MIT OR Apache-2.0` is under both
#
# A page ends at its `<!-- book-nav -->` line: what follows is the footer
# testing/booknav.py writes for a reader on GitHub, and the rendered book
# has its own navigation. booknav also writes README.md's contents, in the
# order and with the chapter numbers this script renders.
#
# In a hand-written page and in a port's kfs.md alike, a link is written
# relative to its own file. A link to another page of docs/kfs becomes a
# link to that page's anchor;
# a link into docs/kdos or anywhere else in the tree becomes a link to the
# file on GitHub. A link to an anchor that does not exist is printed as a
# warning and rendered as its text, because typst refuses a document with a
# link to a missing label. A code span is found as CommonMark finds it (a
# run of N backquotes closes only at the next run of exactly N), and a link
# that starts inside one is code and stays as written. Only prose is read
# for anchors and links: a fenced block's `${#var}` is no anchor. A relative
# `.md` link still in the prose after mapping is a warning, since it would
# be a broken href in the rendered book.
# ---------------------------------

import argparse
import json
import os
import re
import shutil
import subprocess
import sys

GITHUB = "https://github.com/kunaldawn/kdos/blob/main/"
RELEASES = "https://github.com/kunaldawn/kdos/releases/download/"
BOOK = "docs/kfs"
WORK_DIR = "/var/cache/kpkg/work"

# ──────────────────────────────────────────────────────────────────────────
# Markdown escaping

MD_SPECIAL = set("\\`*_{}[]()<>#+-.!|~^$@%&:=\"'")


def esc(s):
    """Recipe text as literal Markdown: every character pandoc could read as
    syntax is backslash-escaped, and runs of whitespace are one space."""
    s = " ".join(str(s).split())
    return "".join("\\" + c if c in MD_SPECIAL else c for c in s)


CODE_SPAN = re.compile(r"(`+)(.+?)\1")


def esc_keep_code(s):
    """Prose from a comment: backquoted spans stay code, the rest escaped."""
    out, pos = [], 0
    s = " ".join(str(s).split())
    for m in CODE_SPAN.finditer(s):
        out.append(esc(s[pos:m.start()]) if m.start() > pos else "")
        if s[pos:m.start()].endswith(" "):
            out.append(" ")
        out.append(code(m.group(2)))
        pos = m.end()
        if s[pos:pos + 1] == " ":
            out.append(" ")
    out.append(esc(s[pos:]))
    return "".join(out).replace("  ", " ")


def code(s):
    """An inline code span that holds any text, backquotes included."""
    s = " ".join(str(s).split())
    run = max((len(m) for m in re.findall(r"`+", s)), default=0)
    tick = "`" * (run + 1)
    pad = " " if s.startswith("`") or s.endswith("`") else ""
    return tick + pad + s + pad + tick


def fence(text, lang=""):
    """A fenced code block longer than any backquote run inside it."""
    run = max((len(m) for m in re.findall(r"`+", text)), default=0)
    tick = "`" * max(3, run + 1)
    body = text.rstrip("\n")
    return "%s%s\n%s\n%s\n" % (tick, lang, body, tick)


def split_fences(md):
    """Markdown cut into (is_code, text) runs at fenced blocks. A fence of N
    backquotes or tildes closes only at a run of at least N of the same
    character, so a fence quoted inside a longer one stays code."""
    out, cur, opener = [], [], None
    for line in md.split("\n"):
        m = re.match(r"^\s*(`{3,}|~{3,})", line)
        if opener is None and m:
            if cur:
                out.append((False, "\n".join(cur)))
            cur, opener = [line], m.group(1)
            continue
        cur.append(line)
        if (opener is not None and m and m.group(1)[0] == opener[0]
                and len(m.group(1)) >= len(opener) and not line.strip()[len(m.group(1)):].strip()):
            out.append((True, "\n".join(cur)))
            cur, opener = [], None
    if cur:
        out.append((opener is not None, "\n".join(cur)))
    return out


def join_fences(runs):
    return "\n".join(t for _, t in runs)


def kfs_list(paras):
    """Paragraphs that are lists of names, in a .kfs-list div: kfs.lua sets
    them unjustified in the PDF, where justifying a line of code spans opens
    wide gaps between the names."""
    if not paras:
        return ""
    return "::: {.kfs-list}\n" + "\n\n".join(paras) + "\n:::\n"


def link(text_md, url):
    return "[%s](<%s>)" % (text_md, url)


def files_n(n):
    """`1 file`, `54 files`: a count from the snapshot, escaped."""
    return "%s %s" % (esc(n), "file" if str(n).strip() == "1" else "files")


def human_bytes(n):
    try:
        n = int(n)
    except (TypeError, ValueError):
        return ""
    for unit in ("B", "KiB", "MiB", "GiB"):
        if n < 1024 or unit == "GiB":
            return ("%d %s" % (n, unit)) if unit == "B" else ("%.1f %s" % (n, unit))
        n /= 1024.0
    return ""


def roman(n):
    vals = [(10, "X"), (9, "IX"), (5, "V"), (4, "IV"), (1, "I")]
    out = ""
    for v, s in vals:
        while n >= v:
            out += s
            n -= v
    return out


def anchor_id(name):
    """A port's stable anchor: `port-<name>`, with `+` spelled `x`, because
    neither a pandoc identifier nor a typst label may carry a `+`."""
    return "port-" + name.replace("+", "x")


def slug(s):
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", s).strip("-")


def github_anchor(text):
    """The id GitHub gives a heading, as mkbook.py and anchorcheck compute it."""
    t = re.sub(r"!?\[((?:[^\[\]]|\[[^\]]*\])*)\]\([^)]*\)", r"\1", text)
    t = t.replace("`", "").replace("**", "").replace("*", "")
    t = t.strip().lower()
    t = re.sub(r"[^\w\- ]", "", t)
    return t.replace(" ", "-")


# ──────────────────────────────────────────────────────────────────────────
# Recipes, by decl.c's rules


def _glob(pat, s):
    p = i = 0
    star, mark = -1, 0
    while i < len(s):
        if p < len(pat) and (pat[p] == "?" or pat[p] == s[i]):
            p += 1
            i += 1
        elif p < len(pat) and pat[p] == "*":
            star, p, mark = p, p + 1, i
        elif star != -1:
            p, mark = star + 1, mark + 1
            i = mark
        else:
            return False
    while p < len(pat) and pat[p] == "*":
        p += 1
    return p == len(pat)


def _strip(val, pat, trailing, longest):
    n = len(val)
    if not trailing:
        for k in range(n + 1):
            cut = n - k if longest else k
            if _glob(pat, val[:cut]):
                return val[cut:]
        return val
    for k in range(n + 1):
        keep = k if longest else n - k
        if _glob(pat, val[keep:]):
            return val[:keep]
    return val


def _subst(val, pat, every):
    needle, _, rep = pat.partition("/")
    if not needle:
        return val
    return val.replace(needle, rep) if every else val.replace(needle, rep, 1)


IDENT = re.compile(r"[A-Za-z0-9_]*")


def expand(text, lookup):
    out, i = [], 0
    while i < len(text):
        c = text[i]
        if c != "$":
            out.append(c)
            i += 1
            continue
        i += 1
        if i >= len(text) or text[i] != "{":
            m = IDENT.match(text, i)
            var = m.group(0)[:63]
            i = m.end()
            v = lookup(var)
            if v is not None:
                out.append(v)
            continue
        i += 1
        m = IDENT.match(text, i)
        var = m.group(0)[:63]
        i = m.end()
        close = text.find("}", i)
        if close < 0:
            break
        op = text[i:close]
        i = close + 1
        v = lookup(var)
        if v is None:
            continue
        if not op:
            out.append(v)
        elif op[0] == "#":
            longest = op[1:2] == "#"
            out.append(_strip(v, op[1 + longest:], False, longest))
        elif op[0] == "%":
            longest = op[1:2] == "%"
            out.append(_strip(v, op[1 + longest:], True, longest))
        elif op[0] == "/":
            every = op[1:2] == "/"
            out.append(_subst(v, op[1 + every:], every))
        elif op[0] == ":":
            spec = op[1:]
            off_s, colon, take_s = spec.partition(":")
            try:
                off = max(0, int(off_s or 0))
            except ValueError:
                off = 0
            off = min(off, len(v))
            want = len(v) - off
            if colon:
                try:
                    want = min(want, max(0, int(take_s or 0)))
                except ValueError:
                    pass
            out.append(v[off:off + want])
        else:
            out.append(v)
    return "".join(out)


KEYS = ("name", "version", "release", "source", "sha256", "description",
        "homepage", "depends", "vendoring", "pypackages", "secdb", "replaces",
        "bench", "bench_setup", "license")
APPEND = {"source", "sha256", "depends", "pypackages"}


def parse_recipe(path):
    """kp_decl_parse: keys, helpers in order, and the raw text."""
    d = {k: "" for k in KEYS}
    helpers = []
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()

    def lookup(var):
        if var in ("name", "version", "release"):
            return d[var]
        for k, v in helpers:
            if k == var:
                return v
        return None

    for raw in text.split("\n"):
        line = raw.strip(" \t\r")
        if not line or line.startswith("#"):
            continue
        m = re.match(r"([^ \t=]*)[ \t]*=[ \t]*(.*)$", line)
        if not m or not re.match(r"[A-Za-z_][A-Za-z0-9_]*$", m.group(1)):
            continue
        key, val = m.group(1), expand(m.group(2), lookup)
        if key in d:
            if key in APPEND and d[key]:
                d[key] += " " + val
            else:
                d[key] = val
        else:
            helpers.append((key, val))
    d["helpers"] = helpers
    return d


def source_file(name, version, src, first):
    """kp_source_file: the name a source entry is cached under."""
    if "::" in src:
        return src.split("::", 1)[0]
    if not re.match(r"(https?|ftp)://", src):
        return src
    base = src.rstrip("/").rsplit("/", 1)[-1]
    if not first:
        return base
    for suf, ext in ((".tar.gz", "tar.gz"), (".tgz", "tar.gz"),
                     (".tar.bz2", "tar.bz2"), (".tbz2", "tar.bz2"),
                     (".tar.xz", "tar.xz"), (".txz", "tar.xz"),
                     (".tar.zst", "tar.zst"), (".zip", "zip")):
        if src.endswith(suf):
            return "%s-%s.%s" % (name, version, ext)
    return base


def is_tarball(f):
    return f.endswith((".tar.gz", ".tgz", ".tar.bz2", ".tbz2", ".tar.xz",
                       ".txz", ".tar"))


BANNER_LINE = re.compile(r"^\s*(#|/?\*|//)?\s*([█╗╔╝╚═║]|-{5,}|KD's Homebrew)")


def strip_banner(text):
    """A script without its shebang and its KDOS banner block."""
    lines = text.split("\n")
    i = 0
    if lines and lines[0].startswith("#!"):
        i = 1
    j = i
    while j < len(lines) and not lines[j].strip():
        j += 1
    if j < len(lines) and "██" in lines[j]:
        while j < len(lines) and (lines[j].startswith("#") or not lines[j].strip()):
            if not lines[j].strip() or BANNER_LINE.match(lines[j]) \
                    or lines[j].strip() == "#":
                j += 1
                continue
            break
        i = j
    while i < len(lines) and not lines[i].strip():
        i += 1
    return "\n".join(lines[i:]).rstrip() + "\n"


def parse_patch(path):
    """(subject, body) from a patch's mail header; (None, None) without one."""
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            head = f.read(65536)
    except OSError:
        return None, None
    lines = head.split("\n")
    subj, body, i = None, [], 0
    while i < len(lines):
        ln = lines[i]
        if ln.startswith("Subject:"):
            s = ln[8:].strip()
            while i + 1 < len(lines) and lines[i + 1][:1] in (" ", "\t") \
                    and lines[i + 1].strip():
                i += 1
                s += " " + lines[i].strip()
            subj = re.sub(r"^\[[^\]]*PATCH[^\]]*\]\s*", "", s)
            i += 1
            while i < len(lines) and lines[i].strip() and \
                    re.match(r"[A-Za-z-]+:", lines[i]):
                i += 1
            while i < len(lines):
                ln = lines[i]
                if ln.startswith("---") or ln.startswith("diff ") or \
                        ln.startswith("Index: "):
                    break
                body.append(ln)
                i += 1
            break
        if ln.startswith("diff ") or ln.startswith("--- "):
            break
        i += 1
    if subj is None:
        return None, None
    text = "\n".join(body).strip()
    text = re.sub(r"\n?(Signed-off-by|Co-authored-by|Reviewed-by):.*", "", text)
    return subj, text.strip()


def parse_sidecar(path):
    """kfs.md's sections, by `## Title`, as Markdown text. A `## ` line
    inside a fenced block is code, not a heading; testing/kfscheck.py
    refuses a section that repeats."""
    out = {}
    if not os.path.isfile(path):
        return out
    cur, fen = None, False
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if FENCE_RE.match(line):
                fen = not fen
            m = None if fen else re.match(r"^##\s+(.*?)\s*$", line)
            if m:
                cur = m.group(1).lower()
                out[cur] = ""
                continue
            if cur is not None:
                out[cur] += line
    return {k: v.strip() for k, v in out.items() if v.strip()}


FLAG_RE = re.compile(r"(?<![\w$/-])(--?[A-Za-z][\w.+-]*(?:=[^\s\\'\"]+)?|-D[A-Za-z_][\w]*=[^\s\\'\"]+)")


ASSIGN_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*=\S*$")
FLAGS_ASSIGN = re.compile(r"^\s*(?:export\s+)?[A-Z_]*FLAGS=(.*)$")
BUILD_TOOLS = ("cmake", "meson")


def build_commands(text):
    """The configure, cmake and meson invocations of a script, and the
    values it gives a *FLAGS variable, each one logical line: comments
    dropped, `\\` continuations joined, split at `&&`, `||` and `;`."""
    joined, cur = [], ""
    for line in text.split("\n"):
        s = "" if line.lstrip().startswith("#") else line.split(" #", 1)[0]
        if s.rstrip().endswith("\\"):
            cur += s.rstrip()[:-1] + " "
            continue
        joined.append(cur + s)
        cur = ""
    joined.append(cur)
    out = []
    for line in joined:
        m = FLAGS_ASSIGN.match(line)
        if m:
            out.append(m.group(1))
            continue
        for cmd in re.split(r"&&|\|\||;", line):
            words = cmd.split()
            while words and (ASSIGN_RE.match(words[0]) or words[0] in ("env", "exec", "command")):
                words.pop(0)
            if not words:
                continue
            tool = os.path.basename(words[0].strip("\"'"))
            if tool.endswith("configure") or tool in BUILD_TOOLS:
                out.append(" ".join(words[1:]))
    return out


def flags_of(text):
    """The switches a script hands its configure, cmake or meson, and the
    defines in its *FLAGS: `--x`, `--x=v`, `-DX` and `-DX=v`. A switch of any other command (`install -Dm755`,
    `patch --no-backup-if-mismatch`) says nothing about how the source is
    configured, so the stage boxes would list it as a difference."""
    out = []
    for cmd in build_commands(text):
        for m in FLAG_RE.finditer(cmd):
            f = m.group(1)
            if f.startswith("--") or f.startswith("-D"):
                out.append(f)
    seen, res = set(), []
    for f in out:
        if f not in seen:
            seen.add(f)
            res.append(f)
    return res


# ──────────────────────────────────────────────────────────────────────────
# The tree


class Port:
    def __init__(self, root, rel, shelf, area=None):
        self.rel = rel
        self.dir = os.path.join(root, rel)
        self.shelf = shelf
        self.area = area
        self.decl = parse_recipe(os.path.join(self.dir, "kpkgbuild"))
        self.name = os.path.basename(rel)
        self.id = anchor_id(self.name)
        self.build = self._read("build.sh")
        self.postinstall = self._read("postinstall.sh")
        self.patches = []
        self.extra = []
        sha_files = set(self.decl["sha256"].split()[1::2])
        for f in sorted(os.listdir(self.dir)):
            p = os.path.join(self.dir, f)
            if f.endswith(".patch"):
                s, b = parse_patch(p)
                self.patches.append((f, s, b))
            elif f not in ("kpkgbuild", "build.sh", "postinstall.sh",
                           "kfs.md") and f not in sha_files:
                self.extra.append(f + ("/" if os.path.isdir(p) else ""))
        self.sidecar = parse_sidecar(os.path.join(self.dir, "kfs.md"))
        self.depends = self.decl["depends"].split()
        self.rdeps = []
        self.phase = None
        self.position = None
        self.why = None
        self.tier = "C"
        self.reasons = []
        self.stages = []

    def _read(self, f):
        p = os.path.join(self.dir, f)
        if not os.path.isfile(p):
            return None
        with open(p, encoding="utf-8", errors="replace") as fh:
            return fh.read()

    @property
    def chroot_dir(self):
        if self.area:
            return "/kdos/" + self.rel
        return "/" + self.rel

    def body_lines(self):
        if not self.build:
            return 0
        return sum(1 for ln in strip_banner(self.build).split("\n") if ln.strip())


def _recipe_file(n):
    return n in ("kpkgbuild", "build.sh", "postinstall.sh") or (len(n) > 6 and n.endswith(".patch"))


def _hash_tree(h, top, rel, depth, named):
    if depth > 8:
        return
    here = os.path.join(top, rel) if rel else top
    try:
        names = sorted(os.listdir(here))
    except OSError:
        return
    for n in names:
        if n == "__pycache__":
            continue
        if not rel and (_recipe_file(n) or n in named or n == "kfs.md"):
            continue
        sub = (rel + "/" + n) if rel else n
        p = os.path.join(top, sub)
        if os.path.isdir(p):
            _hash_tree(h, top, sub, depth + 1, named)
            continue
        try:
            with open(p, "rb") as f:
                data = f.read()
        except OSError:
            continue
        h.update(b"%s %d\n" % (sub.encode(), len(data)))
        h.update(data)


def recipe_hash(portdir):
    """kp_recipe_hash over the same files, with kfs.md and every __pycache__
    left out: the recipe
    files by name, then every other file no `sha256 =` line names, and for a
    port with no source the shared libraries at ../../libs."""
    import hashlib
    h = hashlib.sha256()
    names = sorted(os.listdir(portdir))
    any_ = False
    for n in names:
        if not _recipe_file(n):
            continue
        with open(os.path.join(portdir, n), "rb") as f:
            data = f.read()
        h.update(b"%s %d\n" % (n.encode(), len(data)))
        h.update(data)
        any_ = True
    if not any_:
        return None
    decl = parse_recipe(os.path.join(portdir, "kpkgbuild"))
    if decl["source"]:
        _hash_tree(h, portdir, "", 0, set(decl["sha256"].split()[1::2]))
    else:
        _hash_tree(h, portdir, "", 0, set())
        libs = os.path.join(portdir, "../../libs")
        if os.path.isdir(libs):
            _hash_tree(h, os.path.normpath(libs), "", 0, set())
    return h.hexdigest()


def links_into_docs(root, portdir):
    """Does a top-level symlink of the port resolve under <root>/docs? Such
    a port ships a book, and its recipe hash covers that book's prose."""
    docs = os.path.realpath(os.path.join(root, "docs")) + os.sep
    for n in os.listdir(portdir):
        p = os.path.join(portdir, n)
        if os.path.islink(p) and (os.path.realpath(p) + os.sep).startswith(docs):
            return True
    return False


def include_targets(root):
    """Every file a docs/kfs page pulls in with kfs:include, outside docs/."""
    out = set()
    for dp, dn, fn in os.walk(os.path.join(root, BOOK)):
        for f in fn:
            if not f.endswith(".md"):
                continue
            fen = False
            with open(os.path.join(dp, f), encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    if FENCE_RE.match(line):
                        fen = not fen
                        continue
                    m = DIRECTIVE.match(line)
                    if fen or not m or m.group(1) != "include" or not m.group(2).split():
                        continue
                    rel = os.path.normpath(m.group(2).split()[0])
                    if not rel.startswith(("docs" + os.sep, "..")):
                        out.add(rel)
    return out


def include_ranges(root):
    """Every kfs:include as (file, first line, last line, page), the file
    from the repository root and the page from docs/kfs; a whole-file
    include covers every line."""
    out = []
    book = os.path.join(root, BOOK)
    for dp, dn, fn in os.walk(book):
        for f in fn:
            if not f.endswith(".md"):
                continue
            fen = False
            path = os.path.join(dp, f)
            with open(path, encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    if FENCE_RE.match(line):
                        fen = not fen
                        continue
                    m = DIRECTIVE.match(line)
                    if fen or not m or m.group(1) != "include" or not m.group(2).split():
                        continue
                    args = m.group(2).split()
                    r = re.search(r"\blines=(\d+)-(\d+)", m.group(2))
                    a, b = (int(r.group(1)), int(r.group(2))) if r else (1, 10 ** 9)
                    out.append((os.path.normpath(args[0]), a, b, os.path.relpath(path, book)))
    return out


def stamp_inputs(root):
    """The files no recipe hash covers that the rendered book reads: each
    port's kfs.md, the phase lists and environments, the 00_cross and
    10_bootstrap step scripts (their chapters and the stage boxes),
    ports/shelves, ports/sources.idx, testing/optdeps.py, fs/etc/os-release
    (the edition's VERSION, on the title page) and every kfs:include target.
    Repository-relative and sorted."""
    out = set(include_targets(root))
    out |= {"ports/shelves", "ports/sources.idx", "testing/optdeps.py", "fs/etc/os-release"}
    phases = os.path.join(root, "script/phases")
    for d in sorted(os.listdir(phases)) if os.path.isdir(phases) else []:
        pd = os.path.join(phases, d)
        if not os.path.isdir(pd):
            continue
        for f in os.listdir(pd):
            if f == "packages.txt" or f.endswith(".env") or (
                    f.endswith(".sh") and d in ("00_cross", "10_bootstrap")):
                out.add("script/phases/%s/%s" % (d, f))
        sub = os.path.join(pd, "packages.d")
        if os.path.isdir(sub):
            out |= {"script/phases/%s/packages.d/%s" % (d, f)
                    for f in os.listdir(sub) if f.endswith(".txt")}
    for p in read_ports(root).values():
        if not links_into_docs(root, p.dir) and os.path.isfile(os.path.join(p.dir, "kfs.md")):
            out.add(p.rel + "/kfs.md")
    return sorted(r for r in out if os.path.isfile(os.path.join(root, r)))


def stamp_digest(root):
    """What docs/kfs/data/recipes.stamp holds: one SHA-256 over every port's
    recipe hash, by port path, then over the bytes of every stamp_inputs
    file. Each of those changes the rendered book and none is in kdos-kfs's
    own hash, so the stamp is the way such a change reaches the port: the
    stale stamp fails kfscheck, and rewriting it changes docs/kfs, which
    kdos-kfs does hash. A port that links a book under docs/ is left out:
    kdos-kfs's hash covers docs/kfs, this stamp among it, so with it in,
    writing the stamp would change the digest and no stamp could ever be
    current; kdos-doc's covers docs/kdos, so every edit to that book's
    prose would stale it. Returns (digest, ports, other files)."""
    import hashlib
    h = hashlib.sha256()
    n = 0
    for p in sorted(read_ports(root).values(), key=lambda p: p.rel):
        if links_into_docs(root, p.dir):
            continue
        rh = recipe_hash(p.dir)
        if rh:
            h.update(("%s %s\n" % (p.rel, rh)).encode())
            n += 1
    files = stamp_inputs(root)
    for rel in files:
        with open(os.path.join(root, rel), "rb") as f:
            data = f.read()
        h.update(b"%s %d\n" % (rel.encode(), len(data)))
        h.update(data)
    return h.hexdigest(), n, len(files)


def read_shelves(root):
    shelves = []
    with open(os.path.join(root, "ports/shelves"), encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            parts = s.split(None, 2)
            sid = parts[0]
            if len(parts) > 1 and (parts[1].isdigit() or parts[1] == "-"):
                desc = parts[2] if len(parts) > 2 else ""
            else:
                desc = " ".join(parts[1:])
            shelves.append((sid, desc))
    return shelves


AREA_DESC = {
    "system": "KDOS's own system programs, from src/system",
    "art": "KDOS's own artwork: theme, icons, cursors, splash",
    "desktop": "KDOS's own desktop: the compositor, the panel and their programs",
    "daemons": "KDOS's own root daemons, from src/daemons",
    "devtools": "KDOS's own build tools, from src/devtools",
}


def read_ports(root):
    ports = {}
    core = os.path.join(root, "ports/core")
    for shelf in sorted(os.listdir(core)):
        d = os.path.join(core, shelf)
        if not os.path.isdir(d):
            continue
        for n in sorted(os.listdir(d)):
            if os.path.isfile(os.path.join(d, n, "kpkgbuild")):
                ports.setdefault(n, Port(root, "ports/core/%s/%s" % (shelf, n), shelf))
    src = os.path.join(root, "src")
    for area in sorted(os.listdir(src)):
        d = os.path.join(src, area)
        if not os.path.isdir(d):
            continue
        for n in sorted(os.listdir(d)):
            if os.path.isfile(os.path.join(d, n, "kpkgbuild")):
                ports.setdefault(n, Port(root, "src/%s/%s" % (area, n),
                                         "src-" + area, area))
    for p in ports.values():
        for dep in p.depends:
            if dep in ports:
                ports[dep].rdeps.append(p.name)
    for p in ports.values():
        p.rdeps.sort(key=str.lower)
    return ports


def read_env_meta(path):
    meta = {}
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                m = re.match(r'^\s*export\s+(KDOS_PHASE_TITLE|KDOS_PHASE_DESC|PORT_REPO)="(.*)"', line)
                if m:
                    meta[m.group(1)] = m.group(2)
    except OSError:
        pass
    return meta


def repo_dirs(root, value):
    out = []
    for r in value.split():
        if r.startswith("/ports/"):
            out.append(os.path.join(root, "ports", r[len("/ports/"):]))
        elif r.startswith("/kdos/"):
            out.append(os.path.join(root, r[len("/kdos/"):]))
        else:
            out.append(os.path.join(root, r.lstrip("/")))
    return out


SHELF_BANNER = re.compile(r"^# [a-z0-9][a-z0-9-]* — ")


def read_list(files):
    """Names in list order, each with the comment block directly above its
    run: comment lines that end on the line before a name, with no blank line
    between, and no banner among them."""
    names = []
    for path in files:
        block, run = [], None
        with open(path, encoding="utf-8") as f:
            lines = f.read().split("\n")
        for n, line in enumerate(lines, 1):
            s = line.strip()
            if not s:
                block, run = [], None
                continue
            if s.startswith("#"):
                if run is not None:
                    block, run = [], None
                block.append(s)
                continue
            if run is None:
                text = block
                if any("██" in b or re.match(r"^# ?-{5,}", b) or SHELF_BANNER.match(b)
                       for b in text):
                    text = []
                run = {"lines": text, "names": [], "file": path, "line": n}
            run["names"].append(s)
            names.append((s, run))
    return names


def comment_paragraphs(lines):
    paras, cur = [], []
    for ln in lines:
        t = ln[1:]
        if t.startswith(" "):
            t = t[1:]
        if not t.strip():
            if cur:
                paras.append(" ".join(cur))
                cur = []
            continue
        cur.append(t.strip())
    if cur:
        paras.append(" ".join(cur))
    return paras


class Phase:
    def __init__(self, root, name):
        self.name = name
        self.dir = os.path.join(root, "script/phases", name)
        meta = read_env_meta(os.path.join(self.dir, "phase.env"))
        self.title = meta.get("KDOS_PHASE_TITLE", name)
        self.desc = meta.get("KDOS_PHASE_DESC", "")
        self.repos = repo_dirs(root, meta.get("PORT_REPO", "/ports/core"))
        self.chroot = False
        try:
            with open(os.path.join(self.dir, "phase.env"), encoding="utf-8") as f:
                self.chroot = bool(re.search(r"^export CHROOT=1", f.read(), re.M))
        except OSError:
            pass
        txt = os.path.join(self.dir, "packages.txt")
        dd = os.path.join(self.dir, "packages.d")
        if os.path.isfile(txt):
            self.files = [txt]
        elif os.path.isdir(dd):
            self.files = [os.path.join(dd, f) for f in sorted(os.listdir(dd))
                          if f.endswith(".txt") and not f.startswith(".")]
        else:
            self.files = []
        self.steps = sorted(f for f in os.listdir(self.dir)
                            if re.match(r"^\d+_.*\.sh$", f))
        self.names = read_list(self.files) if self.files else []
        self.order = []

    @property
    def is_packages(self):
        return bool(self.files)


class Resolver:
    def __init__(self):
        self.index = {}

    def _idx(self, repo):
        if repo not in self.index:
            loose, shelved = {}, {}
            try:
                entries = sorted(os.listdir(repo))
            except OSError:
                entries = []
            for e in entries:
                d = os.path.join(repo, e)
                if e.startswith(".") or not os.path.isdir(d):
                    continue
                if os.path.isfile(os.path.join(d, "kpkgbuild")):
                    loose[e] = d
                    continue
                for n in sorted(os.listdir(d)):
                    p = os.path.join(d, n)
                    if not n.startswith(".") and os.path.isfile(os.path.join(p, "kpkgbuild")):
                        shelved.setdefault(n, p)
            self.index[repo] = (loose, shelved)
        return self.index[repo]

    def find(self, repos, name):
        for r in repos:
            loose, shelved = self._idx(r)
            if name in loose:
                return loose[name]
            if name in shelved:
                return shelved[name]
        return None


def resolve_phases(root, ports):
    base = os.path.join(root, "script/phases")
    phases = [Phase(root, d) for d in sorted(os.listdir(base))
              if os.path.isdir(os.path.join(base, d))]
    res = Resolver()
    installed = {}
    sys.setrecursionlimit(100000)
    for ph in phases:
        if not ph.is_packages:
            continue
        order, seen, runs = [], set(), {}
        for n, run in ph.names:
            if n in seen:
                continue
            seen.add(n)
            if run["lines"]:
                runs[n] = run
            order.append(n)
        visited, emitted, eset = set(), [], set()

        def walk(n):
            pdir = res.find(ph.repos, n)
            if not pdir:
                return
            name = os.path.basename(pdir)
            for dep in (ports[name].depends if name in ports else []):
                if dep in visited or dep in installed:
                    continue
                visited.add(dep)
                if not res.find(ph.repos, dep):
                    continue
                walk(dep)
                if dep not in eset:
                    emitted.append(dep)
                    eset.add(dep)

        for n in order:
            if n in visited or n in installed:
                continue
            visited.add(n)
            if not res.find(ph.repos, n):
                continue
            walk(n)
            if n not in eset:
                emitted.append(n)
                eset.add(n)
        # A port's "why" is the comment in the list of the phase that builds
        # it: a later phase's comment over a re-list says why it is named
        # again, not why it exists.
        for i, p in enumerate(emitted, 1):
            installed[p] = ph.name
            if p in ports:
                ports[p].phase = ph
                ports[p].position = i
                ports[p].why = runs.get(p)
        ph.order = [p for p in emitted if p in ports]
    return phases


def read_sources_idx(root):
    idx = {}
    try:
        with open(os.path.join(root, "ports/sources.idx"), encoding="utf-8") as f:
            for line in f:
                if line.startswith("#") or not line.strip():
                    continue
                t = line.split()
                if len(t) >= 4:
                    idx[t[0]] = (t[1], t[2], t[3])
    except OSError:
        pass
    return idx


def read_tsv(path, ncols=None):
    rows = []
    if not os.path.isfile(path):
        return rows
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            rows.append(line.rstrip("\n").split("\t"))
    return rows


class Data:
    def __init__(self, path):
        self.path = path
        self.packages = {r[0]: r for r in read_tsv(os.path.join(path, "packages.tsv"))}
        self.contents = {r[0]: r for r in read_tsv(os.path.join(path, "contents.tsv"))}
        self.whatis = {}
        for r in read_tsv(os.path.join(path, "whatis.tsv")):
            if len(r) >= 4:
                self.whatis.setdefault(r[3], {})[r[0]] = (r[1], r[2])
        self.pc = {}
        for r in read_tsv(os.path.join(path, "pc.tsv")):
            if len(r) >= 3:
                self.pc.setdefault(r[2], []).append((r[0], r[1]))
        self.sources = {r[0]: r[1] for r in read_tsv(os.path.join(path, "sources.tsv"))
                        if len(r) >= 2}
        self.stamp = {}
        try:
            with open(os.path.join(path, "stamp"), encoding="utf-8") as f:
                for line in f:
                    k, _, v = line.strip().partition(" ")
                    if k and not k.startswith("#"):
                        self.stamp[k] = v.strip()
        except OSError:
            pass
        self.tier_a = set()
        try:
            with open(os.path.join(path, "tier-a.txt"), encoding="utf-8") as f:
                for line in f:
                    s = line.split("#", 1)[0].strip()
                    if s:
                        self.tier_a.add(s)
        except OSError:
            pass


def assign_tiers(ports, data):
    for p in ports.values():
        r = []
        if p.patches:
            r.append("carries %d patch%s" % (len(p.patches), "" if len(p.patches) == 1 else "es"))
        if p.decl["vendoring"] or any("vendor" in f for f in p.decl["sha256"].split()[1::2]):
            r.append("vendors its dependencies")
        if p.body_lines() > 30:
            r.append("has a build.sh of %d lines" % p.body_lines())
        if len(p.rdeps) >= 20:
            r.append("is a dependency of %d ports" % len(p.rdeps))
        p.reasons = r
        if p.name in data.tier_a:
            p.tier = "A"
        elif r:
            p.tier = "B"
        else:
            p.tier = "C"


def has_note(p):
    """A Tier B port's note: its kfs.md has an About or a Notes section.
    testing/kfscheck.py's `notes` check holds a new or changed port to the
    same rule, so a port that passes it never renders as having none."""
    return bool(p.sidecar.get("about") or p.sidecar.get("notes"))


# ──────────────────────────────────────────────────────────────────────────
# Hand-written pages


FENCE_RE = re.compile(r"^\s*(```+|~~~+)")
HEAD_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*(\{[^}]*\})?\s*$")
DIRECTIVE = re.compile(r"^\s*<!--\s*kfs:(\w+)\s*(.*?)\s*-->\s*$")
NAV_MARK = "<!-- book-nav -->"
MDLINK = re.compile(r"(!?)\[((?:[^\[\]]|\[[^\]]*\])*)\]\(([^()\s]+)(\s+\"[^\"]*\")?\)")


class Page:
    def __init__(self, book, path):
        self.path = path
        self.rel = os.path.relpath(path, book)
        with open(path, encoding="utf-8") as f:
            self.text = f.read()
        d, f = os.path.split(self.rel)
        stem = f[:-3]
        if not d:
            self.id = "index" if stem == "README" else slug(stem)
        elif stem == "intro":
            self.id = slug(d)
        else:
            self.id = slug(d + "-" + stem)
        first = self.text.split("\n", 1)[0]
        m = re.match(r"^#\s+(.*)$", first)
        self.title = m.group(1).strip() if m else stem
        self.directives = {}
        fen = False
        for line in self.text.split("\n"):
            if FENCE_RE.match(line):
                fen = not fen
                continue
            m = DIRECTIVE.match(line)
            if m and not fen and m.group(1) != "include":
                self.directives[m.group(1)] = m.group(2)


def range_pin(lines, lo, hi):
    """The pin of lines lo..hi (1-based, inclusive) of a file split at
    newlines: the first 12 hex digits of their SHA-256."""
    import hashlib
    return hashlib.sha256("\n".join(lines[lo - 1:hi]).encode()).hexdigest()[:12]


def find_pin(lines, length, pin):
    """Where a range of `length` lines with this pin now starts, 1-based,
    or None: how a range whose file gained or lost lines above it is
    found again."""
    for lo in range(1, len(lines) - length + 2):
        if range_pin(lines, lo, lo + length - 1) == pin:
            return lo
    return None


def include_args(arg):
    """(path, (A, B) or None, pin or None, the words not understood)."""
    parts = arg.split()
    rng, pin, bad = None, None, []
    for p in parts[1:]:
        m = re.match(r"lines=(\d+)-(\d+)$", p)
        if m:
            rng = (int(m.group(1)), int(m.group(2)))
            continue
        m = re.match(r"pin=([0-9a-f]{12})$", p)
        if m:
            pin = m.group(1)
            continue
        if re.match(r"lang=[a-z0-9]+$", p):
            continue
        bad.append(p)
    return (parts[0] if parts else None), rng, pin, bad


def include_lang(arg):
    """The lang= of an include directive, or None. Not in include_args's
    tuple, which testing/kfscheck.py unpacks."""
    m = re.search(r"(?:^|\s)lang=([a-z0-9]+)(?:\s|$)", arg)
    return m.group(1) if m else None


def include(root, arg, warn):
    rel, rng, pin, _ = include_args(arg)
    if not rel:
        warn("empty kfs:include")
        return ""
    path = os.path.join(root, rel)
    if not os.path.isfile(path):
        warn("kfs:include names %s, which does not exist" % rel)
        return "\n*(%s is not in this tree.)*\n" % esc(rel)
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    if rng:
        lines = text.split("\n")
        count = len(lines) - (1 if lines and lines[-1] == "" else 0)
        if not 1 <= rng[0] <= rng[1] <= count:
            warn("kfs:include %s lines=%d-%d: not a range within the file's %d lines"
                 % (rel, rng[0], rng[1], count))
        elif pin and range_pin(lines, *rng) != pin:
            warn("kfs:include %s lines=%d-%d: those lines are not the ones the page was "
                 "written against (pin %s); testing/kfscheck.py --only includes says where "
                 "they went" % (rel, rng[0], rng[1], pin))
        text = "\n".join(lines[rng[0] - 1:rng[1]])
        label = "%s, lines %d–%d" % (rel, rng[0], rng[1])
    else:
        text = strip_banner(text)
        label = rel
    ext = os.path.splitext(rel)[1].lstrip(".")
    lang = {"sh": "bash", "env": "bash", "py": "python", "c": "c", "h": "c",
            "conf": "", "md": "markdown", "patch": "diff"}.get(ext, "")
    if os.path.basename(rel) in ("kpkgbuild",):
        lang = ""
    lang = include_lang(arg) or lang
    return "\n" + fence(text, lang) + "\n*%s*\n" % link(code(label), GITHUB + rel)


class Book:
    def __init__(self, root, data):
        self.root = root
        self.data = data
        self.warnings = []
        self.ids = set()
        self.units = []
        self.chunks = []
        self.page_ids = {}
        self.page_titles = {}
        # Directive name -> function(argument text) -> Markdown, for the
        # directives a page expands in place other than include.
        self.generators = {}

    def warn(self, msg):
        self.warnings.append(msg)

    def add_unit(self, uid, title, level, kind, number="", parent=None, md=""):
        if uid in self.ids:
            self.warn("duplicate id %s" % uid)
        self.ids.add(uid)
        self.units.append({"id": uid, "title": title, "level": level,
                           "kind": kind, "number": number, "parent": parent})
        self.chunks.append((uid, md))

    def rewrite_page(self, page, base_level, part_intro=False):
        """A hand-written page as book Markdown: headings shifted and given
        page-unique ids, directives expanded, links mapped. Returns (title,
        body) where body excludes the title line."""
        lines = page.text.split("\n")
        out, fen, seen = [], False, {}
        page_dir = os.path.dirname(page.path)
        for i, line in enumerate(lines):
            if i == 0 and line.startswith("# "):
                continue
            if FENCE_RE.match(line):
                fen = not fen
                out.append(line)
                continue
            if fen:
                out.append(line)
                continue
            if line.strip() == NAV_MARK:
                break
            m = DIRECTIVE.match(line)
            if m:
                if m.group(1) == "include":
                    out.append(include(self.root, m.group(2),
                                       lambda s: self.warn("%s: %s" % (page.rel, s))))
                elif m.group(1) in self.generators:
                    out.append("\n" + self.generators[m.group(1)](m.group(2)) + "\n")
                continue
            m = HEAD_RE.match(line)
            if m:
                level = len(m.group(1))
                if part_intro:
                    new = base_level + level
                else:
                    new = base_level + level - 1
                new = min(new, 6)
                text = m.group(2)
                a = github_anchor(text)
                if a in seen:
                    seen[a] += 1
                    a = "%s-%d" % (a, seen[a])
                else:
                    seen[a] = 0
                attrs = m.group(3) or ""
                cls = " ".join(c for c in re.findall(r"\.[\w-]+", attrs))
                hid = "%s--%s" % (page.id, a)
                self.ids.add(hid)
                out.append("%s %s {#%s%s}" % ("#" * new, self.map_links(page, page_dir, text),
                                              hid, (" " + cls) if cls else ""))
                continue
            out.append(self.map_links(page, page_dir, line))
        return "\n".join(out).strip() + "\n"

    def map_links(self, page, page_dir, line):
        if "](" not in line:
            return line
        # A link that starts inside a code span is code and stays as written;
        # one whose text holds a code span is still a link, so the line is
        # not split at the spans before matching.
        spans = code_spans(line)

        def sub(m):
            if any(a < m.start() < b for a, b in spans):
                return m.group(0)
            return self._link(page, page_dir, m)
        return MDLINK.sub(sub, line)

    def _link(self, page, page_dir, m):
        bang, text, target, title = m.group(1), m.group(2), m.group(3), m.group(4) or ""
        if target.startswith("<") and target.endswith(">"):
            target = target[1:-1]
        if re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", target):
            return m.group(0)
        path, _, frag = target.partition("#")
        if not path:
            if frag.startswith(("port-", "shelf-", "phase-")) or frag in self.page_ids.values():
                return "%s[%s](#%s%s)" % (bang, text, frag, title)
            return "%s[%s](#%s--%s%s)" % (bang, text, page.id, frag, title)
        absf = os.path.normpath(os.path.join(page_dir, path))
        book = os.path.join(self.root, BOOK)
        if absf.startswith(book + os.sep) and absf.endswith(".md"):
            pid = self.page_ids.get(os.path.relpath(absf, book))
            if pid is None:
                self.warn("%s: link to %s, which is not a page" % (page.rel, target))
                return text
            if frag:
                return "%s[%s](#%s--%s%s)" % (bang, text, pid, frag, title)
            return "%s[%s](#%s%s)" % (bang, text, pid, title)
        relr = os.path.relpath(absf, self.root)
        if relr.startswith(".."):
            self.warn("%s: link %s leaves the tree" % (page.rel, target))
            return text
        url = GITHUB + relr + (("#" + frag) if frag else "")
        return "%s[%s](<%s>%s)" % (bang, text, url, title)


def code_spans(line):
    """The (start, end) of each code span in a line, as CommonMark reads
    them: a run of N backticks opens a span that only the next run of
    exactly N closes, so ``a`b`` is one span; a run with no closer is
    literal text."""
    runs = [(m.start(), m.end()) for m in re.finditer(r"`+", line)]
    out, i = [], 0
    while i < len(runs):
        a, b = runs[i]
        for j in range(i + 1, len(runs)):
            if runs[j][1] - runs[j][0] == b - a:
                out.append((a, runs[j][1]))
                i = j + 1
                break
        else:
            i += 1
    return out


# ──────────────────────────────────────────────────────────────────────────
# Package pages


def port_link(ports, name):
    if name in ports:
        return "[%s](#%s)" % (esc(name), ports[name].id)
    return esc(name)


def kpkg_line(p):
    if p.phase is None or not p.phase.chroot:
        return "kpkg install %s" % p.name
    return ("script/chroot/exec.sh bash -c 'source script/phases/%s/phase.env && kpkg install %s'"
            % (p.phase.name, p.name))


class Gen:
    def __init__(self, root, data, ports, phases, shelves, idx, book, optional):
        self.root = root
        self.data = data
        self.ports = ports
        self.phases = phases
        self.shelves = dict(shelves)
        self.idx = idx
        self.book = book
        self.optional = optional
        self.quoted = include_ranges(root)
        for port in ports.values():
            self.map_note_links(port)

    def map_note_links(self, p):
        """A note's links are written relative to its own file, so they work
        on GitHub; map them as a hand-written page's are, outside code."""
        if not p.sidecar:
            return

        class Note:
            rel = p.rel + "/kfs.md"
            id = p.id
        for k, text in p.sidecar.items():
            out, fen = [], False
            for line in text.split("\n"):
                if FENCE_RE.match(line):
                    fen = not fen
                elif not fen:
                    line = self.book.map_links(Note, p.dir, line)
                out.append(line)
            p.sidecar[k] = "\n".join(out)

    def facts(self, p):
        d = p.decl
        rows = []
        rows.append(("Version", "%s, release %s" % (esc(d["version"]), esc(d["release"]))))
        if p.area:
            rows.append(("Source tree", link(code(p.rel + "/"), GITHUB + p.rel)))
        else:
            rows.append(("Shelf", "%s — %s" % (code(p.shelf), esc(self.shelves.get(p.shelf, "")))))
        if p.phase:
            rows.append(("Phase", "[%s](#phase-%s) (%s), %s of %s in build order"
                         % (esc(p.phase.name), p.phase.name, esc(p.phase.title),
                            p.position, len(p.phase.order))))
        else:
            rows.append(("Phase", "none: no phase list names it"))
        if d["license"]:
            rows.append(("Licence", code(d["license"])))
        if d["homepage"]:
            rows.append(("Homepage", "<%s>" % d["homepage"]))
        pk = self.data.packages.get(p.name)
        if pk and len(pk) >= 6:
            if pk[5] and pk[5] != "-":
                rows.append(("Build time", self.sbu_text(pk)))
            rows.append(("Installed size", "%s in %s" % (esc(human_bytes(pk[3])), files_n(pk[4]))))
        rows.append(("Recipe", link(code(p.rel + "/kpkgbuild"), GITHUB + p.rel + "/kpkgbuild")))
        # Every value is already a cell: text through esc(), whose `\|` is a
        # pipe to pandoc, and code spans, inside which a pipe ends no cell.
        # A second escape would print the backslash or end the cell early.
        out = ["| | |", "|:--------------|:------------------------------------------------------------------|"]
        for k, v in rows:
            out.append("| %s | %s |" % (k, v))
        return "\n".join(out) + "\n"

    def stale(self, p):
        pk = self.data.packages.get(p.name)
        if not pk or len(pk) < 3:
            return ""
        if pk[1] != p.decl["version"] or pk[2] != p.decl["release"]:
            return ("::: {.kfs-note}\nThe build-state snapshot was taken from %s-%s; "
                    "this recipe is %s-%s, so the size, time and contents below describe "
                    "the earlier build.\n:::\n" % (esc(pk[1]), esc(pk[2]),
                                                    esc(p.decl["version"]), esc(p.decl["release"])))
        return ""

    def why(self, p):
        run = p.why
        if not run:
            return ""
        paras = comment_paragraphs(run["lines"])
        if not paras:
            return ""
        # A comment of a few words, such as "The compositor.", says less
        # than the page's own description.
        if len(paras) == 1 and len(paras[0].split()) <= 5:
            return ""
        rel = os.path.relpath(run["file"], self.root)
        names = run["names"]
        if len(names) == 1:
            whom = "it"
        elif len(names) == 2:
            whom = "%s and %s" % (esc(names[0]), esc(names[1]))
        else:
            whom = "the %d ports from %s to %s" % (len(names), esc(names[0]), esc(names[-1]))
        # A comment a hand-written page already quotes is linked, not
        # repeated on every port it heads.
        first, last = run["line"] - len(run["lines"]), run["line"] - 1
        for f, a, b, page in self.quoted:
            pid = self.book.page_ids.get(page) if self.book else None
            if pid and f == rel and a <= first and last <= b:
                return ("The comment above %s in %s is quoted in [%s](#%s).\n"
                        % (whom, link(code(rel), GITHUB + rel),
                           esc(self.book.page_titles.get(page, page)), pid))
        where = "The comment above %s in %s:" % (whom, link(code(rel), GITHUB + rel))
        body = "\n>\n".join("> " + esc_keep_code(x) for x in paras)
        return where + "\n\n" + body + "\n"

    def sources(self, p):
        d = p.decl
        srcs = d["source"].split()
        urls = {}
        for i, s in enumerate(srcs):
            f = source_file(d["name"], d["version"], s, i == 0)
            url = s.split("::", 1)[1] if "::" in s else s
            if re.match(r"(https?|ftp)://", url):
                urls[f] = url
        toks = d["sha256"].split()
        pairs = list(zip(toks[0::2], toks[1::2]))
        if not pairs and not srcs:
            return "None: the port builds from its own directory, %s.\n" % code(p.chroot_dir)
        out = []
        for hx, f in pairs:
            item = ["- %s" % code(f)]
            size = self.data.sources.get(hx)
            if size:
                item[0] += " (%s)" % esc(human_bytes(size))
            item.append("  sha256 %s" % code(hx))
            if f in urls:
                item.append("  upstream: <%s>" % urls[f])
            elif "vendor" in f:
                item.append("  a vendor bundle, made by %s, not an upstream file"
                            % link(code("ports/hackage-vendor"), GITHUB + "ports/hackage-vendor")
                            if p.decl["vendoring"] == "haskell" else
                            "  a vendor bundle: the dependencies upstream's own tool would download, packed once")
            if hx in self.idx:
                tag, asset, _ = self.idx[hx]
                item.append("  archive copy: %s" % link(esc("%s/%s" % (tag, asset)), RELEASES + tag + "/" + asset))
            out.append(" \\\n".join(item))
        for f, url in urls.items():
            if f not in dict((b, a) for a, b in pairs):
                out.append("- %s, <%s> (no sha256 line names it)" % (code(f), url))
        return "\n".join(out) + "\n"

    def deps(self, p):
        out = []
        if p.depends:
            out.append("**Depends on:** " + ", ".join(port_link(self.ports, x) for x in p.depends) + ".")
        else:
            out.append("**Depends on:** nothing a recipe names.")
        if p.rdeps:
            out.append("**Required by** (%d): %s." % (len(p.rdeps), ", ".join(
                port_link(self.ports, x) for x in p.rdeps)))
        else:
            out.append("**Required by:** no port names it.")
        return "\n\n".join(out) + "\n"

    def optdeps(self, p):
        hits = self.optional.get(p.name)
        if not hits:
            return ""
        noted = set(re.findall(r"^[-*]\s+`([^`]+)`", p.sidecar.get("flags", ""), re.M))
        out = ["These switches in %s turn off a feature whose library is a port here. The "
               "recipe may turn each off on purpose, or because the library was not yet a "
               "port when the recipe was written; this list cannot tell which.\n"
               % code("build.sh")]
        for no, sw, target in hits:
            why = (" (explained with the switches below)"
                   if sw in noted or sw.split("=")[0] in noted else "")
            out.append("- %s, line %d: %s%s" % (code(sw), no, port_link(self.ports, target), why))
        return "\n".join(out) + "\n"

    def glossary_line(self):
        """The pointer to the Flag glossary after a page's build.sh, which
        explains the common switches of each build system."""
        pid = self.book.page_ids.get("17-appendices/flag-glossary.md") if self.book else None
        if not pid:
            return ""
        return ("The common switches of each build system are explained in the "
                "[Flag glossary](#%s).\n" % pid)

    def patches(self, p):
        """The patch list: each patch's own Subject: and body, or the note's
        one-line `- `file` — text` bullet for a patch without one. A
        paragraph of the note's Patches section that is not a bullet is
        prose, kept before the list or after it as it stands in the note."""
        notes, before, after = {}, [], []
        for para in re.split(r"\n\s*\n", p.sidecar.get("patches", "")):
            if not para.strip():
                continue
            if re.match(r"^[-*]\s", para):
                for m in re.finditer(r"^-\s+`([^`]+)`\s+—\s+(.*)$", para, re.M):
                    notes[m.group(1)] = m.group(2)
            else:
                (after if notes else before).append(para.strip())
        out = [x + "\n" for x in before]
        for f, subj, body in p.patches:
            url = GITHUB + p.rel + "/" + f
            if subj:
                out.append("- %s — %s" % (link(code(f), url), esc(subj)))
                if body:
                    for para in [x for x in re.split(r"\n\s*\n", body) if x.strip()]:
                        out.append("\n    " + esc(para))
            elif f in notes:
                out.append("- %s — %s" % (link(code(f), url), notes[f]))
            else:
                out.append("- %s — undescribed: the patch carries no %s header"
                           % (link(code(f), url), code("Subject:")))
        out += ["\n" + x for x in after]
        return "\n".join(out) + "\n"

    def stage_box(self, p):
        if not p.stages:
            return ""
        out = []
        pf = set(flags_of(strip_banner(p.build or "")))
        for script, pid, title in p.stages:
            with open(os.path.join(self.root, script), encoding="utf-8") as f:
                sf = set(flags_of(f.read()))
            only_s = sorted(sf - pf)
            only_p = sorted(pf - sf)
            out.append("::: {.kfs-note}")
            stage = ("[%s](#%s)" % (esc(title), pid)) if pid else code(script)
            out.append("**Differs from the bootstrap build.** %s builds this source too, in "
                       "%s, before kpkg exists." % (stage, link(code(script), GITHUB + script)))
            if only_s:
                out.append("\nOnly in the bootstrap build: " + ", ".join(code(x) for x in only_s) + ".")
            if only_p:
                out.append("\nOnly in this build: " + ", ".join(code(x) for x in only_p) + ".")
            if not only_s and not only_p:
                out.append("\nThe two name the same switches.")
            out.append(":::")
        return "\n".join(out) + "\n"

    def body_block(self, p):
        if p.build is None:
            return "*The port has no build.sh.*\n"
        return fence(strip_banner(p.build), "bash")

    def what_kpkg_does(self, p):
        d = p.decl
        name, ver = d["name"], d["version"]
        src_root = "%s/%s" % (WORK_DIR, name)
        out = []
        out.append("kpkg runs these steps, in this order, as root inside the chroot. "
                   "The port directory is %s.\n" % code(p.chroot_dir))
        toks = d["sha256"].split()
        pairs = list(zip(toks[0::2], toks[1::2]))
        out.append("**1. Verify every declared source.** Each file a %s line names, found "
                   "in the port directory or in %s, must hash to its line; a mismatch stops "
                   "the build before anything is unpacked.\n"
                   % (code("sha256 ="), code("/var/cache/kpkg/sources")))
        if pairs:
            out.append(fence("cd %s\n" % p.chroot_dir +
                             "\n".join("echo '%s  %s' | sha256sum -c -" % (h, f) for h, f in pairs), "bash"))
        out.append("**2. Unpack.** The work directory is emptied and the first source is "
                   "unpacked into %s with its top directory stripped; any later tarball "
                   "lands unstripped beside it in %s, and a file that is not a tarball is "
                   "copied into %s.\n" % (code("$SRC"), code("$SRC_ROOT"), code("$SRC")))
        cmds = ["SRC_ROOT=%s" % src_root, "SRC=$SRC_ROOT/%s-%s" % (name, ver),
                "PKG=$SRC_ROOT/pkg", "rm -rf \"$SRC_ROOT\"", "mkdir -p \"$SRC\" \"$PKG\""]
        for i, s in enumerate(d["source"].split()):
            f = source_file(name, ver, s, i == 0)
            if is_tarball(f):
                cmds.append("tar --no-same-permissions --warning=no-unknown-keyword -xf %s -C %s%s"
                            % (f, "\"$SRC\"" if i == 0 else "\"$SRC_ROOT\"",
                               " --strip-components=1" if i == 0 else ""))
            else:
                cmds.append("cp %s \"$SRC\"" % f)
        out.append(fence("\n".join(cmds), "bash"))
        out.append("Every %s and %s older than automake's is then replaced with "
                   "automake's, so a configure script recognises the musl target.\n"
                   % (code("config.guess"), code("config.sub")))
        helpers = ", ".join(code(k) for k, _ in d["helpers"])
        out.append("**3. Build.** %s is sourced in a subshell under %s, from %s, with "
                   "%s, %s, %s%s and the five paths %s, %s, %s, %s and %s set as shell "
                   "variables. Its body, verbatim:\n"
                   % (code("build.sh"), code("set -e"), code("$SRC"), code("name"),
                      code("version"), code("release"),
                      (" and the recipe's helpers " + helpers) if helpers else "",
                      code("PKGNAME"), code("PORT_SRC"), code("SRC_ROOT"), code("SRC"), code("PKG")))
        out.append(self.body_block(p))
        flags = p.sidecar.get("flags")
        if flags:
            out.append("The switches, and why each is set:\n\n" + flags + "\n")
        g = self.glossary_line()
        if g:
            out.append(g)
        if p.postinstall is not None:
            out.append("%s runs on the installed system after the package is adopted:\n"
                       % code("postinstall.sh"))
            out.append(fence(strip_banner(p.postinstall), "bash"))
        out.append("**4. Package.** kpkg removes libtool archives, the info directory "
                   "index, %s and every %s from %s, copies the source's licence files into "
                   "%s, prunes what the image does not ship, lints the tree and rolls it "
                   "reproducibly:\n" % (code("mimeinfo.cache"), code("fonts.dir"), code("$PKG"),
                                        code("/usr/share/licenses/%s" % name)))
        out.append(fence("tar --sort=name --format=gnu --numeric-owner --owner=0 --group=0 \\\n"
                         "    --mtime=@$SOURCE_DATE_EPOCH \\\n"
                         "    --use-compress-program='xz -9 -T0 --block-size=32MiB --no-adjust' \\\n"
                         "    -cf %s-%s-%s.tar.xz -C \"$PKG\" ." % (name, ver, d["release"]), "bash"))
        out.append("**5. Adopt.** kpkgadd installs the package's files over the root, "
                   "refusing a path another package owns, records the file list in "
                   "%s, and runs the triggers its paths call for.\n"
                   % code("/var/lib/kpkg/db/%s" % name))
        return "\n".join(out)

    def contents(self, p, compact=False):
        c = self.data.contents.get(p.name)
        if not c:
            if self.data.packages:
                return ("*Not in the build-state snapshot: %s.*\n"
                        % ("no phase installs it" if p.phase is None else
                           "the snapshot's tree had not built it"))
            return ""
        progs = c[1].split() if len(c) > 1 else []
        libs = c[2].split() if len(c) > 2 else []
        dirs = c[3].split() if len(c) > 3 else []
        wi = self.data.whatis.get(p.name, {})
        pcs = self.data.pc.get(p.name, [])
        out = []
        if compact:
            if progs:
                out.append("**Programs:** " + ", ".join(code(x) for x in progs) + ".")
            if libs:
                out.append("**Libraries:** " + ", ".join(code(x) for x in libs) + ".")
            if dirs:
                out.append("**Directories:** " + ", ".join(code("/" + x) for x in dirs) + ".")
            return kfs_list(out)
        if progs:
            out.append("**Installed programs:** " + ", ".join(code(x) for x in progs) + ".")
        if libs:
            out.append("**Installed libraries:** " + ", ".join(code(x) for x in libs) + ".")
        if dirs:
            out.append("**Installed directories:** " + ", ".join(code("/" + x) for x in dirs) + ".")
        out = [kfs_list(out).rstrip("\n")] if out else []
        desc = [(x, wi[x][1]) for x in progs if x in wi and wi[x][1]]
        if desc or pcs:
            out.append("**Short descriptions**\n")
            rows = ["| | |", "|:--------------|:------------------------------------------------------------------|"]
            for x, dd in desc:
                rows.append("| %s | %s |" % (code(x), esc(dd)))
            for n, dd in pcs:
                if dd:
                    rows.append("| %s | %s |" % (code(n + ".pc"), esc(dd)))
            out.append("\n".join(rows))
        return "\n\n".join(out) + ("\n" if out else "")

    def filelist(self, p):
        return "::: {.kfs-html-only .kfs-filelist port=\"%s\"}\n:::\n" % p.name

    def heading(self, p, number, level):
        d = p.decl
        return ("%s %s. %s-%s {#%s}"
                % ("#" * level, number, esc(d["name"] or p.name), esc(d["version"]), p.id))

    def lead(self, p):
        """The description, carrying the port's index entry."""
        return '[]{.idx term="%s"}%s\n' % (p.name.replace('"', ""), esc(p.decl["description"]))

    def full_page(self, p, number):
        h = lambda t, s: "#### %s {#%s--%s}\n\n%s" % (t, p.id, s, "")
        parts = [self.heading(p, number, 3), ""]
        parts.append(self.lead(p))
        parts.append(self.stale(p))
        parts.append(self.facts(p))
        if p.tier == "B" and p.reasons and not has_note(p):
            parts.append("::: {.kfs-note}\nThis port %s; a page like this one carries a "
                         "note in %s saying why, and this one has none yet.\n:::\n"
                         % (esc("; ".join(p.reasons)), code(p.rel + "/kfs.md")))
        if "about" in p.sidecar:
            parts.append(h("About", "about") + p.sidecar["about"] + "\n")
        w = self.why(p)
        if w:
            parts.append(h("Why it is in KDOS", "why") + w)
        parts.append(h("Sources", "sources") + self.sources(p))
        parts.append(h("Dependencies", "depends") + self.deps(p))
        o = self.optdeps(p)
        if o:
            parts.append(h("Optional, off in KDOS", "optional") + o)
        if p.patches:
            parts.append(h("Patches", "patches") +
                         "build.sh applies these; kpkg applies nothing by itself.\n\n" + self.patches(p))
        sb = self.stage_box(p)
        if sb:
            parts.append(sb)
        parts.append(h("Installation", "install") +
                     "From the repository root, inside the build container, this is the "
                     "line kdosbuild runs for the port:\n\n" + fence(kpkg_line(p), "bash"))
        parts.append(h("What kpkg does", "steps") + self.what_kpkg_does(p))
        if p.extra:
            parts.append("Other files in the port directory, read by %s: %s.\n"
                         % (code("build.sh"), ", ".join(code(x) for x in p.extra)))
        ct = self.contents(p)
        if ct:
            parts.append(h("Contents", "contents") + ct)
        parts.append(self.filelist(p))
        if "notes" in p.sidecar:
            parts.append(h("Notes", "notes") + p.sidecar["notes"] + "\n")
        return "::: {.kfs-page}\n" + "\n".join(x for x in parts if x is not None) + "\n:::\n"

    def card(self, p, number):
        d = p.decl
        parts = [self.heading(p, number, 3), ""]
        parts.append(self.lead(p))
        parts.append(self.stale(p))
        facts = ["%s-%s" % (esc(d["version"]), esc(d["release"]))]
        if d["license"]:
            facts.append(code(d["license"]))
        pk = self.data.packages.get(p.name)
        if pk and len(pk) >= 6:
            if pk[5] and pk[5] != "-":
                facts.append(self.sbu_text(pk))
            facts.append("%s, %s" % (esc(human_bytes(pk[3])), files_n(pk[4])))
        if p.phase:
            facts.append("%s, %s of %s" % (esc(p.phase.name), p.position, len(p.phase.order)))
        facts.append(link(code(p.rel), GITHUB + p.rel))
        parts.append(" · ".join(facts) + "\n")
        if d["homepage"]:
            parts.append("**Homepage:** <%s>\n" % d["homepage"])
        w = self.why(p)
        if w:
            parts.append("**Why it is in KDOS.** " + w)
        if "about" in p.sidecar:
            parts.append(p.sidecar["about"] + "\n")
        parts.append(self.deps(p))
        toks = d["sha256"].split()
        if toks:
            parts.append("**Sources:** " + ", ".join(
                "%s%s" % (code(f), (" (%s)" % esc(human_bytes(self.data.sources[h])))
                          if h in self.data.sources else "")
                for h, f in zip(toks[0::2], toks[1::2])) +
                ". [Every step kpkg takes](#04-chroot-kpkg-install) applies to this port.\n")
        sb = self.stage_box(p)
        if sb:
            parts.append(sb)
        o = self.optdeps(p)
        if o:
            parts.append("**Optional, off in KDOS.** " + o)
        parts.append("**Install:** " + code(kpkg_line(p)) + "\n")
        parts.append("**build.sh:**\n\n" + self.body_block(p))
        flags = p.sidecar.get("flags")
        if flags:
            parts.append(flags + "\n")
        ct = self.contents(p, compact=True)
        if ct:
            parts.append(ct)
        parts.append(self.filelist(p))
        if "notes" in p.sidecar:
            parts.append(p.sidecar["notes"] + "\n")
        return "::: {.kfs-card}\n" + "\n".join(x for x in parts if x) + "\n:::\n"

    # ── Generated appendices: the kfs:sbu and kfs:licences directives

    def sbu_text(self, pk):
        """A port's build time, saying so when its figure was measured on
        another machine than the SBU it is divided by, or names none."""
        unit_m = self.data.stamp.get("sbu-machine", "-") or "-"
        m = pk[6] if len(pk) >= 7 and pk[6] else "-"
        if unit_m != "-" and m == "-":
            return "%s SBU, on a machine its figure does not name" % esc(pk[5])
        if unit_m != "-" and m != unit_m:
            return "%s SBU, measured on another machine than the SBU" % esc(pk[5])
        return "%s SBU" % esc(pk[5])

    def sbu_tables(self, arg=""):
        st = self.data.stamp
        unit = st.get("sbu-seconds", "-")
        measured = {n: r for n, r in self.data.packages.items()
                    if len(r) >= 6 and r[5] not in ("", "-")}
        out = []
        head = ("The snapshot was gathered from a built tree that had %s packages installed, "
                "the newest of them dated %s, while the checkout stood at commit %s."
                % (esc(st.get("packages", "an unknown number of")),
                   esc(st.get("date", "unknown")), code(st.get("commit", "unknown"))))
        unit_m = st.get("sbu-machine", "-") or "-"

        def mach(m):
            return st.get("machine-%s" % m, "")

        if unit not in ("", "-"):
            head += " One SBU, the time `00_cross/binutils` took there, was %s seconds" % esc(unit)
            if mach(unit_m):
                head += ", measured on machine %s: %s." % (esc(unit_m), esc(mach(unit_m)))
            else:
                head += (", in a figure that names no machine: the kdosbuild that measured it "
                         "filed none.")
        else:
            head += " It records no SBU, so no time below can be given."
        out.append(head)

        # Every port figure is divided by that one SBU, whichever run and
        # machine measured it, so a figure from another machine is a ratio
        # of two machines' speeds and is marked as one.
        def where(r):
            return r[6] if len(r) >= 7 and r[6] else "-"

        others = sorted({where(r) for r in measured.values()} - {unit_m},
                        key=lambda m: (m == "-", m))
        line = "%d of those packages have a measured time" % len(measured)
        if measured and unit_m == "-":
            named = sorted({where(r) for r in measured.values()} - {"-"})
            line += (". The SBU's own figure names no machine, so whether a figure shares the "
                     "SBU's machine cannot be told")
            if named:
                line += "; %s" % "; ".join(
                    "%d name machine %s (%s)"
                    % (sum(1 for r in measured.values() if where(r) == m), esc(m),
                       esc(mach(m) or "unknown")) for m in named)
            line += "."
        elif measured and not others:
            line += ", every one on the machine that measured the SBU."
        elif measured:
            parts = []
            for m in others:
                k = sum(1 for r in measured.values() if where(r) == m)
                if m == "-":
                    parts.append("%d in figures that name no machine" % k)
                else:
                    parts.append("%d on machine %s (%s)" % (k, esc(m), esc(mach(m) or "unknown")))
            line += (". The figures come from more than one run, and not all of them from the "
                     "SBU's machine: %s. Those are marked \\* below; each is still divided by the "
                     "one SBU, so it is a ratio across two machines, or for a figure that names "
                     "none may be, not a ratio of one machine's times." % "; ".join(parts))
        else:
            line += "."
        out.append(line)
        done = [ph for ph in self.phases if any(n in measured for n in ph.order)]
        bare = [ph for ph in self.phases if ph.order and not any(n in measured for n in ph.order)]
        if done:
            out.append("The package phases with measured ports are %s."
                       % ", ".join(code(ph.name) for ph in done))
        for ph in done:
            rows = [n for n in ph.order if n in measured]
            total = sum(float(measured[n][5]) for n in rows)
            name = esc(ph.name)
            if "phase-" + ph.name in self.book.ids:
                name = "[%s](#phase-%s)" % (name, ph.name)
            if len(rows) == len(ph.order):
                cover = "all %d ports measured" % len(rows)
            else:
                cover = ("%d of %d ports measured, so the phase costs more than the measured "
                         "ones' sum" % (len(rows), len(ph.order)))
            out.append("**%s**, %s: %s, %.1f SBU in all."
                       % (name, esc(ph.title), cover, total))
            t = ["| Port | Version | SBU |", "|:-------------------------|:---------------|------:|"]
            for n in rows:
                r = measured[n]
                mark = "" if unit_m == "-" or where(r) == unit_m else " \\*"
                t.append("| %s | %s | %s%s |" % (port_link(self.ports, n),
                                                 esc(r[1] + "-" + r[2]), esc(r[5]), mark))
            out.append("\n".join(t))
        if bare and measured:
            out.append("No port of %s has a measured time in the snapshot."
                       % ", ".join(code(ph.name) for ph in bare))
        if not measured:
            out.append("*The snapshot holds no measured build time.*")
        return "\n\n".join(out) + "\n"

    def licence_tables(self, arg=""):
        ids, exc = {}, {}
        for n in sorted(self.ports, key=str.lower):
            expr = self.ports[n].decl["license"]
            toks = [t for t in re.split(r"[()\s]+", expr) if t]
            for i, t in enumerate(toks):
                if t in ("AND", "OR", "WITH"):
                    continue
                into = exc if i > 0 and toks[i - 1] == "WITH" else ids
                lst = into.setdefault(t, [])
                if n not in lst:
                    lst.append(n)
        none = sorted((n for n in self.ports if not self.ports[n].decl["license"].strip()),
                      key=str.lower)
        out = []

        def table(d, label):
            t = ["| %s | Ports | |" % label,
                 "|:-------------------|----:|:-------------------------------------------------|"]
            for k in sorted(d, key=lambda k: (-len(d[k]), k.lower())):
                t.append("| %s | %d | %s |" % (code(k), len(d[k]),
                                               ", ".join(port_link(self.ports, n) for n in d[k])))
            return "\n".join(t)

        out.append("%d licence ids are named by the %d recipes, and %d exceptions."
                   % (len(ids), len(self.ports), len(exc)))
        if ids:
            out.append(table(ids, "Licence"))
        if exc:
            out.append("The exceptions, each named after `WITH` in an expression:")
            out.append(table(exc, "Exception"))
        if none:
            out.append("No `license =` line: %s." % ", ".join(port_link(self.ports, n) for n in none))
        return "\n\n".join(out) + "\n"


def load_optional(root, ports):
    """testing/optdeps.py's matches, when the tree carries it."""
    path = os.path.join(root, "testing/optdeps.py")
    if not os.path.isfile(path):
        return {}
    import importlib.util
    spec = importlib.util.spec_from_file_location("kfs_optdeps", path)
    mod = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(mod)
    except Exception:
        return {}
    names = {n: p.dir for n, p in ports.items()}
    lower = {n.lower(): n for n in names}
    out = {}
    for n, p in ports.items():
        b = os.path.join(p.dir, "build.sh")
        if not os.path.isfile(b):
            continue
        for no, sw, keys in mod.switches(b):
            for k in keys:
                t = mod.resolve(k, names, lower)
                if t and t != n:
                    out.setdefault(n, []).append((no, sw, t))
                    break
    return out


# ──────────────────────────────────────────────────────────────────────────
# Assembly


def part_dirs(book):
    return sorted(d for d in os.listdir(book)
                  if os.path.isdir(os.path.join(book, d)) and re.match(r"^\d\d-", d))


ORDER_TOKEN = re.compile(r"\[([^\]]+)\]|(\S+)")


def order_groups(intro):
    """The kfs:order directive as [(label or None, page stem)]: each stem
    with the `[Label]` written last before it."""
    label, out = None, []
    for m in ORDER_TOKEN.finditer(intro.directives.get("order", "") if intro else ""):
        if m.group(1):
            label = m.group(1).strip()
        else:
            out.append((label, m.group(2)))
    return out


def page_order(book, d, intro):
    files = sorted(f for f in os.listdir(os.path.join(book, d))
                   if f.endswith(".md") and f != "intro.md")
    order = [stem for _, stem in order_groups(intro)]
    named = [f + ".md" for f in order if f + ".md" in files]
    return named + [f for f in files if f not in named]


def stage_page_for(script_phase, step):
    stem = step[:-3]
    m = re.match(r"^(\d+)_(.*)$", stem)
    num, rest = (m.group(1), m.group(2)) if m else ("", stem)
    rest = rest.replace("_", "-").replace("++", "xx")
    cands = []
    if num:
        cands.append("%s-%s.md" % (num, rest))
    cands.append(rest + ".md")
    return cands


def edition(root, data):
    version = ""
    try:
        with open(os.path.join(root, "fs/etc/os-release"), encoding="utf-8") as f:
            for line in f:
                if line.startswith("VERSION="):
                    version = line.split("=", 1)[1].strip().strip('"')
    except OSError:
        pass
    commit = os.environ.get("KDOS_GIT_COMMIT", "")
    date = os.environ.get("KDOS_GIT_DATE", "")
    when = os.environ.get("SOURCE_DATE_EPOCH", "") or os.environ.get("KDOS_GIT_TIME", "")
    if not commit or not date or not when:
        try:
            if not commit:
                commit = subprocess.run(["git", "-C", root, "rev-parse", "--short", "HEAD"],
                                        capture_output=True, text=True).stdout.strip()
            if not date:
                date = subprocess.run(["git", "-C", root, "log", "-1", "--format=%cs"],
                                      capture_output=True, text=True).stdout.strip()
            if not when:
                when = subprocess.run(["git", "-C", root, "log", "-1", "--format=%ct"],
                                      capture_output=True, text=True).stdout.strip()
        except OSError:
            pass
    try:
        epoch = int(when)
    except ValueError:
        epoch = 0
    # epoch: the commit time of the tree rendered, which kfsrender dates the
    # tarball and the PDF by; 0 for a tree with no git and no time given.
    return {"version": version or "unknown", "commit": commit or "unknown",
            "date": date or "unknown", "epoch": epoch, "snapshot": data.stamp}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--data")
    ap.add_argument("--out")
    a = ap.parse_args()
    root = os.path.abspath(a.root)
    data = Data(a.data or os.path.join(root, BOOK, "data"))
    out = a.out or os.path.join(root, "build/kfs/src")
    bookdir = os.path.join(root, BOOK)

    shelves = read_shelves(root)
    ports = read_ports(root)
    phases = resolve_phases(root, ports)
    assign_tiers(ports, data)
    idx = read_sources_idx(root)
    optional = load_optional(root, ports)
    phase_by = {ph.name: ph for ph in phases}

    book = Book(root, data)
    for n in sorted(data.tier_a - set(ports)):
        book.warn("tier-a.txt names %s, which is not a port" % n)
    pages = {}
    for d in [""] + part_dirs(bookdir):
        full = os.path.join(bookdir, d)
        for f in sorted(os.listdir(full)):
            if f.endswith(".md"):
                pg = Page(bookdir, os.path.join(full, f))
                pages[pg.rel] = pg
                book.page_ids[pg.rel] = pg.id
                book.page_titles[pg.rel] = pg.title

    # Each bootstrap script's page, and the port its first extract names.
    stage_pages = {}
    for d in part_dirs(bookdir):
        intro = pages.get(os.path.join(d, "intro.md"))
        ph = phase_by.get(intro.directives.get("phase", "")) if intro else None
        if not ph or ph.is_packages:
            continue
        for step in ph.steps:
            for c in stage_page_for(ph.name, step):
                pg = pages.get(os.path.join(d, c))
                if pg:
                    stage_pages[(ph.name, step)] = pg
                    break
    for ph in phases:
        for step in ph.steps:
            path = "script/phases/%s/%s" % (ph.name, step)
            with open(os.path.join(root, path), encoding="utf-8", errors="replace") as f:
                m = re.search(r"extract_port_source\s+([A-Za-z0-9+_.-]+)", f.read())
            if m and m.group(1) in ports:
                pg = stage_pages.get((ph.name, step))
                ports[m.group(1)].stages.append((path, pg.id if pg else None,
                                                 pg.title if pg else step))

    gen = Gen(root, data, ports, phases, shelves, idx, book, optional)
    book.generators = {"sbu": gen.sbu_tables, "licences": gen.licence_tables}
    ed = edition(root, data)
    shelf_rank = {s: i for i, (s, _) in enumerate(shelves)}

    readme = pages.get("README.md")
    if readme:
        # The HTML landing page; the PDF's title page and contents stand in
        # for it there.
        # README.md's contents section is written for a reader on GitHub,
        # where the generated chapters have no page to link; the rendered
        # home page points at contents.html, where every unit is linked.
        home = re.sub(r"(?ms)^(#+ Contents\b[^\n]*\n).*?^<!-- /book-contents -->[^\n]*\n",
                      lambda m: m.group(1) + "\nEvery part, chapter and package page is linked "
                      "from the <a href=\"contents.html\">Contents</a> page.\n",
                      book.rewrite_page(readme, 2), count=1)
        book.add_unit("index", readme.title, 0, "home",
                      md="::: {.kfs-html-only}\n# %s {#kfs-home}\n\n%s\n:::\n"
                      % (esc(readme.title), home))

    chapter = 0
    part_no = 0
    placed = set()
    unphased_part = None
    for d in part_dirs(bookdir):
        intro = pages.get(os.path.join(d, "intro.md"))
        if not intro:
            book.warn("%s has no intro.md" % d)
            continue
        front = d.startswith("00-")
        if not front:
            part_no += 1
        ptitle = intro.title if front else "Part %s. %s" % (roman(part_no), intro.title)
        ph = phase_by.get(intro.directives.get("phase", ""))
        pid = intro.id
        md = "# %s {#%s}\n\n" % (book.map_links(intro, os.path.dirname(intro.path), ptitle), pid)
        if ph:
            md = md.rstrip("\n") + "\n\n[]{#phase-%s}\n\n" % ph.name
            book.ids.add("phase-" + ph.name)
        md += book.rewrite_page(intro, 1, part_intro=True)
        book.add_unit(pid, ptitle, 1, "part", number=("" if front else roman(part_no)), md=md)
        if "unphased" in intro.directives:
            unphased_part = pid

        for f in page_order(bookdir, d, intro):
            pg = pages[os.path.join(d, f)]
            if ph and ph.is_packages and f[:-3] in shelf_rank:
                continue
            if front:
                num, t = "", pg.title
            else:
                chapter += 1
                num, t = str(chapter), "%d. %s" % (chapter, pg.title)
            md = "## %s {#%s}\n\n%s" % (book.map_links(pg, os.path.dirname(pg.path), t), pg.id,
                                        book.rewrite_page(pg, 2))
            book.add_unit(pg.id, t, 2, "chapter", number=num, parent=pid, md=md)

        if ph and ph.is_packages:
            groups = {}
            for n in ph.order:
                groups.setdefault(ports[n].shelf, []).append(n)
            keys = sorted(groups, key=lambda s: (shelf_rank.get(s, 10 ** 6), s))
            for s in keys:
                chapter += 1
                cid = "shelf-%s-%s" % (ph.name, s)
                desc = gen.shelves.get(s) or AREA_DESC.get(s[4:], "")
                # The shelf id repeats across phases, so the title names both.
                t = "%d. The %s shelf, %s" % (chapter, s, ph.name)
                md = "## %s {#%s}\n\n%s\n\n" % (esc(t), cid, esc(desc))
                narrative = pages.get(os.path.join(d, s + ".md"))
                if narrative:
                    md += book.rewrite_page(narrative, 2) + "\n"
                rows = ["| | Version | |", "|:----------------|:--------|:------------------------------------------------------|"]
                for n in groups[s]:
                    p = ports[n]
                    rows.append("| [%s](#%s) | %s | %s |" % (esc(n), p.id, esc(p.decl["version"]),
                                                          esc(p.decl["description"])))
                md += ("The ports of this shelf that %s installs, in build order.\n\n"
                       % esc(ph.name)) + "\n".join(rows) + "\n"
                book.add_unit(cid, t, 2, "chapter", number=str(chapter), parent=pid, md=md)
                for k, n in enumerate(groups[s], 1):
                    p = ports[n]
                    num = "%d.%d" % (chapter, k)
                    body = gen.full_page(p, num) if p.tier in "AB" else gen.card(p, num)
                    book.add_unit(p.id, "%s. %s-%s" % (num, n, p.decl["version"]), 3,
                                  "page" if p.tier in "AB" else "card", number=num,
                                  parent=cid, md=body)
                    placed.add(n)

    rest = sorted((n for n in ports if n not in placed), key=str.lower)
    if rest:
        if unphased_part is None:
            book.warn("no part carries kfs:unphased; %d ports have nowhere to go" % len(rest))
        else:
            chapter += 1
            cid = "shelf-unphased"
            t = "%d. Ports no phase builds" % chapter
            md = ("## %s {#%s}\n\nThese recipes are in the tree, and no phase list names "
                  "them. %s names the ones kept on purpose for a machine that wants them; "
                  "any other is a program of KDOS's own that a step script compiles without "
                  "kpkg, as `10_bootstrap` compiles the installer, so that a running system "
                  "can rebuild it from its recipe.\n\n"
                  % (esc(t), cid, link(code("ports/unbuilt"), GITHUB + "ports/unbuilt")))
            # Inserted after the part's own pages, at the end of the book.
            book.add_unit(cid, t, 2, "chapter", number=str(chapter), parent=unphased_part, md=md)
            for k, n in enumerate(rest, 1):
                p = ports[n]
                num = "%d.%d" % (chapter, k)
                body = gen.full_page(p, num) if p.tier in "AB" else gen.card(p, num)
                book.add_unit(p.id, "%s. %s-%s" % (num, n, p.decl["version"]), 3,
                              "page" if p.tier in "AB" else "card", number=num, parent=cid, md=body)
                placed.add(n)

    # Links to anchors that will not exist are rendered as their text. Only
    # prose is read: `${#var}` in a quoted script is no anchor, and a link
    # inside a fence is code. A relative .md link left in prose is one the
    # mapping missed, and it would be a broken href in the rendered book.
    all_ids = set(book.ids)
    for _, md in book.chunks:
        for is_code, t in split_fences(md):
            if not is_code:
                all_ids.update(re.findall(r"\{#([^\s}]+)", t))
    fixed = []
    bad = {}
    for uid, md in book.chunks:
        def fix(m, uid=uid):
            if m.group(3) in all_ids:
                return m.group(0)
            bad.setdefault(uid, []).append(m.group(3))
            return m.group(2)
        runs = []
        for is_code, t in split_fences(md):
            if not is_code:
                t = re.sub(r"(?<!\])(!?)\[((?:[^\[\]]|\[[^\]]*\])*)\]\(#([^)\s]+)(?:\s+\"[^\"]*\")?\)",
                           fix, t)
                for line in t.split("\n"):
                    for m in re.finditer(r"\]\((?!<?[A-Za-z][A-Za-z0-9+.-]*:|<?#)<?([^)\s>]+\.md)"
                                         r"(?:#[^)\s>]*)?>?[)\s]", line):
                        if not any(a < m.start() < b for a, b in code_spans(line)):
                            book.warn("%s: relative link %s was not mapped" % (uid, m.group(1)))
            runs.append((is_code, t))
        fixed.append((uid, join_fences(runs)))
    book.chunks = fixed
    for uid, ids in sorted(bad.items()):
        book.warn("%s: links to missing anchors: %s" % (uid, ", ".join(sorted(set(ids)))))

    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(os.path.join(out, "parts"))
    yaml = ["---",
            "title: \"KDOS from Scratch\"",
            "subtitle: \"How KDOS is built, phase by phase and package by package, and how a KDOS machine is run\"",
            "kfs-version: \"%s\"" % ed["version"].replace('"', ""),
            "kfs-commit: \"%s\"" % ed["commit"].replace('"', ""),
            "kfs-date: \"%s\"" % ed["date"].replace('"', ""),
            "kfs-snapshot: \"%s\"" % (data.stamp.get("commit", "none") + " " +
                                      data.stamp.get("date", "")).strip().replace('"', ""),
            "lang: en",
            "---", ""]
    with open(os.path.join(out, "book.md"), "w", encoding="utf-8") as bf:
        bf.write("\n".join(yaml))
        for i, (uid, md) in enumerate(book.chunks):
            bf.write("\n```{=html}\n<!--kfs-unit %s-->\n```\n\n" % uid)
            bf.write(md.rstrip("\n") + "\n")
            with open(os.path.join(out, "parts", "%04d-%s.md" % (i, uid)), "w", encoding="utf-8") as pf:
                pf.write(md)
    tiers = {"A": 0, "B": 0, "C": 0}
    for p in ports.values():
        tiers[p.tier] += 1
    with open(os.path.join(out, "units.json"), "w", encoding="utf-8") as jf:
        json.dump({"edition": ed, "units": book.units, "ports": len(ports),
                   "placed": len(placed), "tiers": tiers}, jf, indent=1)
    for w in book.warnings:
        print("kfsgen: warning: " + w, file=sys.stderr)
    missing = [n for n in ports if n not in placed]
    print("kfsgen: %d units, %d of %d ports (A %d, B %d, C %d), %d warnings"
          % (len(book.units), len(placed), len(ports), tiers["A"], tiers["B"], tiers["C"],
             len(book.warnings)))
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
