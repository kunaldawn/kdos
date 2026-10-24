#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   kfsrender — kfsgen.py's Markdown as chunked HTML, one PDF and a tarball
#
#     kfsrender.py --pandoc CMD --typst CMD [--src DIR] [--out DIR]
#                  [--buildfs DIR] [--no-html] [--no-pdf]
#
#   --pandoc, --typst  each a command line, split on spaces, so a binary run
#                      through another root's loader is one argument
#   --src     kfsgen.py's output (default: build/kfs/src)
#   --out     where the book goes (default: build/kfs): html/, kfs.pdf,
#             kfs-html.tar.zst
#   --buildfs a built root whose kpkg database supplies every package's
#             full file list, into the HTML only; without a database the
#             pages carry contents.tsv's summary alone
#
# PANDOC RUNS ONCE PER FORMAT over the whole book, never once per page: one
# HTML run whose output is cut at kfsgen's unit markers, and one typst run
# that typst compiles. A link inside the book is `#id`; once the HTML is cut
# into files, each is rewritten to `<file>.html#id` from the ids every file
# defines, so a link never names a file by hand.
#
# THE TYPST COMPILE IS OFFLINE: --ignore-system-fonts and no package import,
# so the PDF uses typst's embedded fonts and nothing the build machine
# happens to have installed. It is untagged (--no-pdf-tags): the structure
# tags of a book this size are two thirds of the file, and the HTML is the
# accessible edition.
#
# The tarball's members are sorted, owned by 0:0 and dated by the edition's
# commit time, which kfsgen records in units.json: SOURCE_DATE_EPOCH, else
# KDOS_GIT_TIME, else `git -C <root> log`, else 0 for a tree with no git.
# One tree gives one tarball, from any working directory.
# ---------------------------------

import argparse
import html
import io
import json
import os
import re
import shlex
import shutil
import string
import subprocess
import sys
import tarfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
BOOK_TITLE = "KDOS from Scratch"
READER = ("markdown-citations-tex_math_dollars-raw_tex-implicit_figures"
          "-subscript-superscript")
UNIT_RE = re.compile(r"<!--kfs-unit ([^\s>]+)-->")
ID_RE = re.compile(r'\sid="([^"]+)"')
HREF_RE = re.compile(r'href="#([^"]+)"')
IDX_RE = re.compile(r'<span id="(idx-\d+)" class="idx"[^>]*?data-term="([^"]*)"')
FILES_RE = re.compile(r'<div class="kfs-html-only kfs-filelist" data-port="([^"]+)">\s*</div>')


def run(cmd, label):
    t = time.time()
    p = subprocess.run(cmd, capture_output=True, text=True)
    noise = [ln for ln in (p.stderr + p.stdout).split("\n") if ln.strip()]
    for ln in noise:
        print("%s: %s" % (label, ln), file=sys.stderr)
    if p.returncode != 0:
        raise SystemExit("%s exited %d" % (label, p.returncode))
    return time.time() - t, len(noise)


# --preserve-tabs in both runs: a build.sh is shown verbatim, and pandoc
# otherwise expands its tabs to four spaces.
def pandoc_html(pandoc, src, out):
    return run(pandoc + [os.path.join(src, "book.md"), "-f", READER, "-t", "html5",
                         "--wrap=none", "--syntax-highlighting=none", "--preserve-tabs",
                         "--lua-filter", os.path.join(HERE, "index.lua"),
                         "--lua-filter", os.path.join(HERE, "kfs.lua"),
                         "-o", out], "pandoc html")


def pandoc_typst(pandoc, src, out):
    return run(pandoc + [os.path.join(src, "book.md"), "-f", READER, "-t", "typst",
                         "-s", "--template", os.path.join(HERE, "kfs.typ"),
                         "--wrap=preserve", "--syntax-highlighting=idiomatic", "--preserve-tabs",
                         "--lua-filter", os.path.join(HERE, "index.lua"),
                         "--lua-filter", os.path.join(HERE, "kfs.lua"),
                         "-o", out], "pandoc typst")


def file_list(buildfs, name):
    if not buildfs:
        return None
    p = os.path.join(buildfs, "var/lib/kpkg/db", name)
    try:
        with open(p, encoding="utf-8", errors="replace") as f:
            lines = f.read().split("\n")[1:]
    except OSError:
        return None
    return [ln[1:] if ln.startswith("./") else ln for ln in lines
            if ln.strip() and not ln.endswith("/")]


def write_html(src, out, body_path, buildfs, units, edition):
    with open(os.path.join(HERE, "template.html"), encoding="utf-8") as f:
        tpl = f.read()
    tpl = string.Template(re.sub(r"^<!--.*?-->\s*", "", tpl, flags=re.S))
    with open(body_path, encoding="utf-8") as f:
        body = f.read()

    pieces = UNIT_RE.split(body)
    chunks = {}
    order = []
    for i in range(1, len(pieces), 2):
        uid = pieces[i]
        text = pieces[i + 1]
        text = re.sub(r"\s*$", "", text)
        chunks[uid] = text
        order.append(uid)
    meta = {u["id"]: u for u in units}
    fname = {uid: ("index.html" if uid == "index" else uid + ".html") for uid in order}
    owner = {}
    for uid in order:
        for i in ID_RE.findall(chunks[uid]):
            owner.setdefault(i, uid)
        owner[uid] = uid

    def fix(uid):
        def sub(m):
            t = m.group(1)
            o = owner.get(t)
            if o is None:
                return m.group(0)
            if o == uid:
                return 'href="#%s"' % t
            if t == o:
                return 'href="%s"' % fname[o]
            return 'href="%s#%s"' % (fname[o], t)
        return sub

    lists = 0

    def files(m):
        nonlocal lists
        fl = file_list(buildfs, m.group(1))
        if not fl:
            return ""
        lists += 1
        return ('<details class="kfs-files"><summary>Every installed file (%d)</summary>'
                '<pre><code>%s</code></pre></details>'
                % (len(fl), html.escape("\n".join(fl))))

    shutil.rmtree(os.path.join(out, "html"), ignore_errors=True)
    hdir = os.path.join(out, "html")
    os.makedirs(hdir)
    shutil.copy(os.path.join(HERE, "kfs.css"), os.path.join(hdir, "kfs.css"))
    ed = "KDOS %s · %s · %s" % (edition["version"], edition["commit"], edition["date"])

    def title_of(uid):
        return meta.get(uid, {}).get("title", uid)

    def crumbs(uid):
        out_ = []
        u = meta.get(uid)
        while u and u.get("parent"):
            p = meta.get(u["parent"])
            if not p:
                break
            out_.insert(0, '<a href="%s">%s</a>' % (fname[p["id"]], html.escape(p["title"])))
            u = p
        if uid != "index":
            out_.insert(0, '<a href="index.html">KDOS from Scratch</a>')
        return " › ".join(out_)

    index_terms = {}
    for k, uid in enumerate(order):
        text = HREF_RE.sub(fix(uid), chunks[uid])
        text = FILES_RE.sub(files, text)
        for iid, term in IDX_RE.findall(text):
            index_terms.setdefault(html.unescape(term), []).append((uid, iid))
        nav = []
        if k > 0:
            p = order[k - 1]
            nav.append('<a class="kfs-prev" href="%s">← %s</a>' % (fname[p], html.escape(title_of(p))))
        par = meta.get(uid, {}).get("parent")
        if par:
            nav.append('<a class="kfs-up" href="%s">↑ %s</a>' % (fname[par], html.escape(title_of(par))))
        if k + 1 < len(order):
            n = order[k + 1]
            nav.append('<a class="kfs-next" href="%s">%s →</a>' % (fname[n], html.escape(title_of(n))))
        t = re.sub(r"^[\dIVXL.]+\s+|^Part [IVXL]+\. ", "", title_of(uid))
        page = tpl.substitute(title=html.escape(t if t == BOOK_TITLE else t + " — " + BOOK_TITLE),
                              kind=meta.get(uid, {}).get("kind", "page"),
                              edition=html.escape(ed), crumbs=crumbs(uid), nav="\n".join(nav),
                              body=text)
        with open(os.path.join(hdir, fname[uid]), "w", encoding="utf-8") as f:
            f.write(page)

    # The contents page: every unit, parts open, chapters' packages folded.
    kids = {}
    for u in units:
        kids.setdefault(u.get("parent"), []).append(u)
    toc = ['<div class="kfs-contents"><ul>']
    for u in units:
        if u["level"] != 1:
            continue
        toc.append('<li class="kfs-part"><a href="%s">%s</a><ul>' % (fname[u["id"]], html.escape(u["title"])))
        for c in kids.get(u["id"], []):
            sub = kids.get(c["id"], [])
            if sub:
                toc.append('<li><details><summary><a href="%s">%s</a> (%d)</summary><ul>'
                           % (fname[c["id"]], html.escape(c["title"]), len(sub)))
                for s in sub:
                    toc.append('<li><a href="%s">%s</a></li>' % (fname[s["id"]], html.escape(s["title"])))
                toc.append("</ul></details></li>")
            else:
                toc.append('<li><a href="%s">%s</a></li>' % (fname[c["id"]], html.escape(c["title"])))
        toc.append("</ul></li>")
    toc.append("</ul></div>")
    page = tpl.substitute(title="Contents — " + BOOK_TITLE, kind="contents", edition=html.escape(ed),
                          crumbs='<a href="index.html">KDOS from Scratch</a>', nav="",
                          body="<h1>Contents</h1>\n" + "\n".join(toc))
    with open(os.path.join(hdir, "contents.html"), "w", encoding="utf-8") as f:
        f.write(page)

    # The index page, from every [term]{.idx} span. Terms that differ only in
    # case are one entry, printed in the spelling most spans use (the first in
    # sort order on a tie), and an entry names each page once at its first
    # span: kfs.typ builds the PDF index by the same rule, and a port page's
    # generated `pipewire` span would otherwise sit apart from the prose's
    # `PipeWire`.
    pos = {uid: k for k, uid in enumerate(order)}
    folded = {}
    for t, refs in index_terms.items():
        e = folded.setdefault(t.lower(), {"names": {}, "refs": []})
        e["names"][t] = e["names"].get(t, 0) + len(refs)
        e["refs"].extend(refs)
    letters = {}
    for f, e in folded.items():
        t = sorted(sorted(e["names"]), key=lambda n: -e["names"][n])[0]
        seen, refs = set(), []
        for uid, iid in sorted(e["refs"], key=lambda r: (pos[r[0]], int(r[1][4:]))):
            if uid not in seen:
                seen.add(uid)
                refs.append((uid, iid))
        k = t[:1].upper()
        letters.setdefault(k if k.isalpha() else "#", []).append((f, t, refs))
    ix = ['<div class="kfs-index">']
    for k in sorted(letters):
        ix.append("<h2>%s</h2><ul>" % html.escape(k))
        for _, t, refs in sorted(letters[k]):
            if len(refs) == 1:
                uid, iid = refs[0]
                ix.append('<li><a href="%s#%s">%s</a></li>' % (fname[uid], iid, html.escape(t)))
            else:
                ix.append("<li>%s: %s</li>" % (html.escape(t), ", ".join(
                    '<a href="%s#%s">%s</a>' % (fname[uid], iid, html.escape(title_of(uid)))
                    for uid, iid in refs)))
        ix.append("</ul>")
    ix.append("</div>")
    page = tpl.substitute(title="Index — " + BOOK_TITLE, kind="genindex", edition=html.escape(ed),
                          crumbs='<a href="index.html">KDOS from Scratch</a>', nav="",
                          body="<h1>Index</h1>\n" + "\n".join(ix))
    with open(os.path.join(hdir, "genindex.html"), "w", encoding="utf-8") as f:
        f.write(page)
    return len(order) + 2, lists


def tarball(out, epoch):
    hdir = os.path.join(out, "html")
    dest = os.path.join(out, "kfs-html.tar.zst")
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.GNU_FORMAT) as tf:
        for base, dirs, fs in os.walk(hdir):
            dirs.sort()
            for f in sorted(fs):
                p = os.path.join(base, f)
                ti = tf.gettarinfo(p, "kfs/" + os.path.relpath(p, hdir))
                ti.uid = ti.gid = 0
                ti.uname = ti.gname = ""
                ti.mtime = epoch
                ti.mode = 0o644
                with open(p, "rb") as fh:
                    tf.addfile(ti, fh)
    data = raw.getvalue()
    try:
        from compression import zstd
        with open(dest, "wb") as f:
            f.write(zstd.compress(data, level=19))
    except ImportError:
        p = subprocess.run(["zstd", "-19", "-q", "-c"], input=data, capture_output=True)
        if p.returncode != 0:
            raise SystemExit("zstd failed: " + p.stderr.decode())
        with open(dest, "wb") as f:
            f.write(p.stdout)
    return dest


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pandoc", required=True)
    ap.add_argument("--typst", required=True)
    ap.add_argument("--src", default="build/kfs/src")
    ap.add_argument("--out", default="build/kfs")
    ap.add_argument("--buildfs")
    ap.add_argument("--no-html", action="store_true")
    ap.add_argument("--no-pdf", action="store_true")
    a = ap.parse_args()
    pandoc = shlex.split(a.pandoc)
    typst = shlex.split(a.typst)
    src, out = os.path.abspath(a.src), os.path.abspath(a.out)
    with open(os.path.join(src, "units.json"), encoding="utf-8") as f:
        meta = json.load(f)
    t0 = time.time()
    tmp = os.path.join(out, "tmp")
    os.makedirs(tmp, exist_ok=True)
    body = os.path.join(tmp, "body.html")
    typ = os.path.join(tmp, "kfs.typ")
    errs = []
    times = {}

    def job(name, fn, *args):
        try:
            times[name] = fn(*args)
        except SystemExit as e:
            errs.append(str(e))

    th = []
    if not a.no_html:
        th.append(threading.Thread(target=job, args=("pandoc html", pandoc_html, pandoc, src, body)))
    if not a.no_pdf:
        th.append(threading.Thread(target=job, args=("pandoc typst", pandoc_typst, pandoc, src, typ)))
    for t in th:
        t.start()
    pdf = os.path.join(out, "kfs.pdf")
    for t in th:
        t.join()
    if errs:
        raise SystemExit("; ".join(errs))

    # The tree's commit time, which kfsgen read from the tree it rendered,
    # never from this process's working directory.
    epoch = int(meta["edition"].get("epoch") or 0)

    def compile_pdf():
        return run(typst + ["compile", "--ignore-system-fonts", "--no-pdf-tags",
                            "--creation-timestamp", str(epoch), "--root", tmp, typ, pdf],
                   "typst")

    th = []
    if not a.no_pdf:
        th.append(threading.Thread(target=job, args=("typst", compile_pdf)))
        th[-1].start()
    if not a.no_html:
        tt = time.time()
        n, lists = write_html(src, out, body, a.buildfs, meta["units"], meta["edition"])
        tar = tarball(out, epoch)
        times["html split"] = (time.time() - tt, 0)
        print("kfsrender: %d HTML pages, %d with full file lists, %s (%d bytes)"
              % (n, lists, os.path.relpath(tar), os.path.getsize(tar)))
    for t in th:
        t.join()
    if errs:
        raise SystemExit("; ".join(errs))
    if not a.no_pdf:
        print("kfsrender: %s (%d bytes)" % (os.path.relpath(pdf), os.path.getsize(pdf)))
    for k, (secs, noise) in times.items():
        print("kfsrender: %-12s %6.1f s, %d diagnostic lines" % (k, secs, noise))
    print("kfsrender: %.1f s in all" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
