#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   make_data — the build-state snapshot KDOS from Scratch renders from
#
#     make_data.py [--root DIR] [--buildfs DIR] [--timings FILE]
#                  [--srccache DIR] [--out DIR]
#     make_data.py --stamp-only [--root DIR] [--out DIR]
#
#   --stamp-only  write recipes.stamp alone, from the repository alone: no
#               built tree is read and the snapshot's other files are left
#               as they are. For a change that moves the stamp and no
#               package (a kfs.md, a phase list, an included script) on a
#               checkout with no build; `make book-stamp` runs it
#   --root      the repository (default: the current directory)
#   --buildfs   a built root (default: <root>/build/fs); its kpkg database,
#               var/lib/kpkg/db, is what is read, with the files it lists
#   --timings   kdosbuild's per-step seconds (default:
#               <buildfs>/../snapshots/timings.json)
#   --srccache  the source cache, for download sizes (default:
#               <root>/ports/.srccache, else the one beside --buildfs)
#   --out       the snapshot directory (default: <root>/docs/kfs/data)
#
# EVERYTHING WRITTEN IS SMALL TEXT, because it is committed. A package's
# full file list is not: the manifests of a built tree are tens of megabytes,
# so contents.tsv keeps a classified summary (programs, libraries, the
# topmost directories no other package shares) and kfsrender.py reads the
# full lists from a live database only, into the HTML only.
#
#   packages.tsv   name, version, release, installed bytes, files, SBU
#   contents.tsv   name, programs, libraries, directories (space-separated)
#   whatis.tsv     program, section, NAME line, owning package (sections
#                  1, 6 and 8, read from the pages themselves: mandoc.db is
#                  binary and 70_image writes the whatis files last)
#   pc.tsv         .pc name, Description:, owning package
#   sources.tsv    sha256, bytes, for every source the cache holds
#   stamp          the checkout's commit when the data was gathered, the
#                  date of the newest installed package, the package count
#                  and the seconds of one SBU
#   recipes.stamp  one SHA-256 over every recipe file kpkg hashes, then
#                  every kfs.md, phase list and environment, bootstrap step
#                  script and kfs:include target, but for the ports that
#                  link a book under docs/ (kfsgen's stamp_digest);
#                  testing/kfscheck.py fails while it is not this tree's
#
# ONE SBU is the seconds `00_cross/binutils` took. A port with no figure in
# timings.json has `-`: a step served from the package store records none.
#
# Only stdlib: the same script runs on the host, in CI and in the chroot.
# ---------------------------------

import argparse
import gzip
import bz2
import lzma
import json
import os
import re
import stat
import subprocess
import sys
import time

# No __pycache__ beside kfsgen.py: script/book ships inside kdos-kfs
# through its `book` link, and the checkout should hold only the files it
# commits. Both recipe hashes skip a cache that does get written.
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kfsgen  # noqa: E402

try:
    from compression import zstd
except ImportError:
    zstd = None

PROG_RE = re.compile(r"^(?:usr/(?:local/)?)?s?bin/[^/]+$")
LIB_RE = re.compile(r"^(?:usr/)?lib(?:64)?/(lib[^/]+?\.(?:so(?:\.[0-9.]+)?|a))$")
MAN_RE = re.compile(r"^usr/(?:local/)?share/man/man([168])[a-z]*/([^/]+)$")
PC_RE = re.compile(r"/pkgconfig/([^/]+)\.pc$")


def read_db(buildfs):
    db = os.path.join(buildfs, "var/lib/kpkg/db")
    out = {}
    for n in sorted(os.listdir(db)):
        p = os.path.join(db, n)
        if n.startswith(".") or not os.path.isfile(p):
            continue
        with open(p, encoding="utf-8", errors="replace") as f:
            lines = f.read().split("\n")
        head = lines[0].split()
        paths = [ln[2:] if ln.startswith("./") else ln for ln in lines[1:] if ln.strip()]
        out[n] = {"version": head[0] if head else "", "release": head[1] if len(head) > 1 else "",
                  "paths": [x for x in paths if x], "mtime": os.path.getmtime(p)}
    return out


def open_page(path):
    with open(path, "rb") as f:
        raw = f.read(1 << 20)
    if raw[:2] == b"\x1f\x8b":
        raw = gzip.decompress(raw)
    elif raw[:6] == b"\xfd7zXZ\x00":
        raw = lzma.decompress(raw)
    elif raw[:3] == b"BZh":
        raw = bz2.decompress(raw)
    elif raw[:4] == b"\x28\xb5\x2f\xfd":
        if zstd is None:
            return ""
        raw = zstd.decompress(raw)
    return raw[:262144].decode("utf-8", errors="replace")


ROFF_ESC = [(r"\\f[BIRP1-4]|\\f\(..|\\f\[[^\]]*\]", ""), (r"\\\(em|\\\[em\]", "—"),
            (r"\\\(en|\\\[en\]", "–"), (r"\\-", "-"), (r"\\\(aq", "'"), (r"\\\(lq|\\\(rq", '"'),
            (r"\\&", ""), (r"\\e", "\\\\"), (r"\\ ", " "), (r"\\\*\(..|\\\*.", ""),
            (r"\\s[-+]?\d", ""), (r"\\\(..", ""), (r"\\\[[^\]]*\]", ""), (r"\\.", "")]


def unroff(s):
    for a, b in ROFF_ESC:
        s = re.sub(a, b, s)
    return " ".join(s.split())


def name_line(text):
    """The one-line description from a page's NAME section, man or mdoc."""
    lines = text.split("\n")
    nd = [ln[4:] for ln in lines if ln.startswith(".Nd ")]
    if nd:
        return unroff(nd[0].strip().strip('"'))
    for i, ln in enumerate(lines):
        if re.match(r'^\.S[Hh]\s+"?(NAME|Name|NOM|BEZEICHNUNG)"?\s*$', ln):
            buf = []
            for nl in lines[i + 1:i + 12]:
                if re.match(r"^\.S[Hh]\b", nl):
                    break
                if nl.startswith((".\\\"", "'\\\"")):
                    continue
                if nl.startswith("."):
                    m = re.match(r"^\.(?:B|I|BR|IR|RB|RI|BI|IB|Nm)\s+(.*)$", nl)
                    if m:
                        buf.append(m.group(1).replace('"', ""))
                    continue
                buf.append(nl)
            s = unroff(" ".join(buf))
            m = re.split(r"\s+[-–—]+\s+", s, maxsplit=1)
            if len(m) == 2:
                return m[1].strip()
            return ""
    return ""


def write_stamp(root, out):
    digest, n, files = kfsgen.stamp_digest(root)
    with open(os.path.join(out, "recipes.stamp"), "w", encoding="utf-8") as f:
        f.write("# One SHA-256 over every recipe file kpkg hashes, then every kfs.md,\n")
        f.write("# phase list and environment, bootstrap step script and kfs:include\n")
        f.write("# target; the ports that link a book under docs/ excluded:\n")
        f.write("# written by make_data.py, compared by testing/kfscheck.py\n")
        f.write("sha256 %s\nports %d\nfiles %d\n" % (digest, n, files))
    return n, files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--buildfs")
    ap.add_argument("--timings")
    ap.add_argument("--srccache")
    ap.add_argument("--out")
    ap.add_argument("--stamp-only", action="store_true")
    a = ap.parse_args()
    root = os.path.abspath(a.root)
    out = a.out or os.path.join(root, kfsgen.BOOK, "data")
    if a.stamp_only:
        os.makedirs(out, exist_ok=True)
        n, files = write_stamp(root, out)
        print("make_data: recipes.stamp over %d recipes and %d other files" % (n, files))
        return 0
    buildfs = os.path.abspath(a.buildfs or os.path.join(root, "build/fs"))
    if not os.path.isdir(os.path.join(buildfs, "var/lib/kpkg/db")):
        print("make_data: %s has no kpkg database (var/lib/kpkg/db): name a built tree with "
              "BUILDFS=, or run make book-stamp to rewrite recipes.stamp alone" % buildfs,
              file=sys.stderr)
        return 1
    timings = a.timings or os.path.join(os.path.dirname(buildfs), "snapshots/timings.json")
    srccache = a.srccache
    if not srccache:
        srccache = os.path.join(root, "ports/.srccache")
        if not os.path.isdir(srccache):
            srccache = os.path.join(os.path.dirname(os.path.dirname(buildfs)), "ports/.srccache")
    os.makedirs(out, exist_ok=True)

    t0 = time.time()
    ports = kfsgen.read_ports(root)
    phases = kfsgen.resolve_phases(root, ports)
    db = read_db(buildfs)

    steps, unit = {}, None
    try:
        with open(timings, encoding="utf-8") as f:
            steps = json.load(f).get("steps", {})
        unit = steps.get("00_cross/binutils")
    except (OSError, ValueError):
        pass

    # Directory owners, so a package's own directories can be told from the
    # shared ones every package lists.
    owners = {}
    for n, e in db.items():
        for p in e["paths"]:
            if p.endswith("/"):
                owners.setdefault(p.rstrip("/"), set()).add(n)

    pk_rows, ct_rows, wi_rows, pc_rows = [], [], [], []
    for n, e in sorted(db.items()):
        size, files = 0, 0
        progs, libs, dirs = [], [], []
        for p in e["paths"]:
            if p.endswith("/"):
                d = p.rstrip("/")
                if owners.get(d) == {n}:
                    parent = os.path.dirname(d)
                    if owners.get(parent) != {n}:
                        dirs.append(d)
                continue
            files += 1
            full = os.path.join(buildfs, p)
            try:
                st = os.lstat(full)
            except OSError:
                st = None
            if st and stat.S_ISREG(st.st_mode):
                size += st.st_size
            if PROG_RE.match(p):
                progs.append(os.path.basename(p))
            m = LIB_RE.match(p)
            if m:
                lib = re.sub(r"\.so(\.[0-9.]+)?$", ".so", m.group(1))
                if lib not in libs:
                    libs.append(lib)
            m = MAN_RE.match(p)
            if m and st and stat.S_ISREG(st.st_mode):
                prog = re.sub(r"\.(gz|xz|bz2|zst)$", "", m.group(2))
                prog = re.sub(r"\.[0-9][a-z0-9]*$", "", prog)
                try:
                    text = open_page(full)
                except (OSError, EOFError, lzma.LZMAError, ValueError):
                    text = ""
                if text and not text.lstrip().startswith(".so "):
                    desc = name_line(text)
                    if desc:
                        wi_rows.append((prog, m.group(1), desc, n))
            m = PC_RE.search("/" + p)
            if m and st and stat.S_ISREG(st.st_mode):
                desc = ""
                try:
                    with open(full, encoding="utf-8", errors="replace") as f:
                        for line in f:
                            if line.startswith("Description:"):
                                desc = line.split(":", 1)[1].strip()
                                break
                except OSError:
                    pass
                pc_rows.append((m.group(1), " ".join(desc.split()), n))
        sbu = "-"
        p = ports.get(n)
        if unit and p is not None and p.phase is not None:
            s = steps.get("%s/%s" % (p.phase.name, n))
            if s is not None:
                sbu = "%.1f" % (s / unit) if s / unit >= 0.1 else "0.1"
        pk_rows.append((n, e["version"], e["release"], str(size), str(files), sbu))
        ct_rows.append((n, " ".join(sorted(set(progs))), " ".join(sorted(libs)),
                        " ".join(sorted(dirs))))

    def clean(s):
        return str(s).replace("\t", " ").replace("\n", " ")

    def write(name, header, rows):
        with open(os.path.join(out, name), "w", encoding="utf-8") as f:
            f.write("# " + "\t".join(header) + "\n")
            for r in rows:
                f.write("\t".join(clean(x) for x in r) + "\n")

    write("packages.tsv", ("name", "version", "release", "bytes", "files", "sbu"), pk_rows)
    write("contents.tsv", ("name", "programs", "libraries", "directories"), ct_rows)
    seen = set()
    wi = []
    for r in sorted(wi_rows):
        if (r[0], r[1], r[3]) not in seen:
            seen.add((r[0], r[1], r[3]))
            wi.append(r)
    write("whatis.tsv", ("program", "section", "description", "package"), wi)
    write("pc.tsv", ("name", "description", "package"), sorted(set(pc_rows)))

    src_rows = []
    if os.path.isdir(srccache):
        for p in ports.values():
            for h in p.decl["sha256"].split()[0::2]:
                f = os.path.join(srccache, "sha256-%s" % h[:2].lower(), h.lower())
                try:
                    src_rows.append((h, str(os.path.getsize(f))))
                except OSError:
                    pass
    write("sources.tsv", ("sha256", "bytes"), sorted(set(src_rows)))

    tree = os.path.dirname(os.path.dirname(buildfs))
    commit = ""
    for g in (tree, root):
        try:
            commit = subprocess.run(["git", "-C", g, "rev-parse", "--short", "HEAD"],
                                    capture_output=True, text=True).stdout.strip()
        except OSError:
            commit = ""
        if commit:
            break
    newest = max((e["mtime"] for e in db.values()), default=time.time())
    with open(os.path.join(out, "stamp"), "w", encoding="utf-8") as f:
        f.write("# The built tree docs/kfs/data was taken from: written by make_data.py\n")
        f.write("commit %s\n" % (commit or "unknown"))
        f.write("date %s\n" % time.strftime("%Y-%m-%d", time.gmtime(newest)))
        f.write("packages %d\n" % len(db))
        f.write("sbu-seconds %s\n" % (("%.2f" % unit) if unit else "-"))

    write_stamp(root, out)

    print("make_data: %d packages, %d whatis, %d pc, %d sources, %.1f s"
          % (len(pk_rows), len(wi), len(set(pc_rows)), len(set(src_rows)), time.time() - t0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
