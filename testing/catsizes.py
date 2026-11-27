#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/catsizes.py — what each catalogue row adds, from apt itself
# ---------------------------------
#
#   python3 testing/catsizes.py [--catalogue FILE] [--engine podman|docker]
#                               [--write] [--rows ID,ID...]
#
# An app or data row's `meta` bytes are what its image adds over its
# parent's, and the store's room check and Discover's size both read them.
# This measures them without building anything: in a debian:trixie-slim
# container pinned to the catalogue's snapshot (the date in the image's own
# sources file when the catalogue says `auto`, as the store takes it), it asks
# `apt-get install -s --no-install-recommends` for the row's whole chain and
# for its parent's chain, and sums the Installed-Size of every package the
# first installs and the second does not. A `deb` row's release asset is
# fetched the way the store's RUN fetches it, and its own Installed-Size is
# read out of the package.
#
# Installed-Size is the unpacked files, which is what a row's figure means;
# the base and runtime rows were measured with `du` of a built image, so they
# are never measured here and --rows skips them. --write rewrites the bytes
# field of every measured app and data row's `meta` line in place. The
# network is needed: the snapshot archive and, for a `deb` row, the release.

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CAT = os.path.join(HERE, "..", "src", "system", "kdos-appbox", "catalogue")
IMAGE = "debian:trixie-slim"
CACHE = os.path.expanduser("~/.cache/kdos-portwork/catsizes")


def parse(path):
    rows, debs, meta = {}, {}, {}
    snapshot = "auto"
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        w = line.split()
        if w[0] == "snapshot" and len(w) >= 3:
            snapshot = w[2]
        elif w[0] in ("base", "runtime", "app", "data") and len(w) >= 3:
            rows[w[1]] = (w[0], "" if w[2] == "-" else w[2], w[3:])
        elif w[0] == "deb" and len(w) >= 4:
            debs[w[1]] = (w[2], w[3])
        elif w[0] == "meta" and len(w) >= 3:
            f = line.split(None, 2)[2].split("|")
            if len(f) >= 3:
                meta[w[1]] = int(f[2] or 0)
    return rows, debs, meta, snapshot


class Box:
    def __init__(self, engine):
        self.engine = engine
        self.name = "kdos-catsizes-%d" % os.getpid()
        subprocess.run([engine, "run", "-d", "--name", self.name, IMAGE,
                        "sleep", "infinity"], check=True,
                       stdout=subprocess.DEVNULL)

    def sh(self, script, check=True):
        r = subprocess.run([self.engine, "exec", self.name, "sh", "-c", script],
                           capture_output=True, text=True)
        if check and r.returncode:
            sys.exit("catsizes: %s\n%s" % (script[:120], r.stderr[-2000:]))
        return r.stdout

    def cp_in(self, src, dst):
        subprocess.run([self.engine, "cp", src, "%s:%s" % (self.name, dst)],
                       check=True)

    def close(self):
        subprocess.run([self.engine, "rm", "-f", self.name],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def fetch_deb(api, pat):
    with urllib.request.urlopen(api, timeout=60) as r:
        rel = json.load(r)
    urls = [a["browser_download_url"] for a in rel.get("assets", [])
            if a["browser_download_url"].endswith(pat)]
    if not urls:
        sys.exit("catsizes: %s lists no asset ending in %s" % (api, pat))
    os.makedirs(CACHE, exist_ok=True)
    out = os.path.join(CACHE, os.path.basename(urls[0]))
    if not os.path.exists(out):
        with urllib.request.urlopen(urls[0], timeout=600) as r, \
                open(out + ".part", "wb") as f:
            shutil.copyfileobj(r, f)
        os.rename(out + ".part", out)
    return out


INST = re.compile(r"^Inst (\S+) \((\S+) .*\[(\S+)\]\)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--catalogue", default=DEFAULT_CAT)
    ap.add_argument("--engine", default=shutil.which("podman") and "podman"
                    or "docker")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--rows", default="")
    o = ap.parse_args()

    rows, debs, meta, snapshot = parse(o.catalogue)
    want = [i for i in rows if rows[i][0] in ("app", "data")]
    if o.rows:
        want = [i for i in o.rows.split(",")
                if i in rows and rows[i][0] in ("app", "data")]

    def chain(i):
        out = []
        while i:
            out.insert(0, i)
            i = rows[i][1]
        return out

    box = Box(o.engine)
    try:
        if snapshot == "auto":
            snapshot = box.sh(
                "sed -n 's|^# *https\\?://snapshot.debian.org/archive/debian/"
                "\\([0-9TZ]*\\)$|\\1|p' /etc/apt/sources.list.d/debian.sources"
            ).strip()
        if snapshot and snapshot != "off":
            box.sh(
                "printf '%%s\\n' 'Types: deb' "
                "'URIs: http://snapshot.debian.org/archive/debian/%s/' "
                "'Suites: trixie trixie-updates' 'Components: main' "
                "'Signed-By: /usr/share/keyrings/debian-archive-keyring.pgp' '' "
                "'Types: deb' "
                "'URIs: http://snapshot.debian.org/archive/debian-security/%s/' "
                "'Suites: trixie-security' 'Components: main' "
                "'Signed-By: /usr/share/keyrings/debian-archive-keyring.pgp' "
                "> /etc/apt/sources.list.d/debian.sources && "
                "echo 'Acquire::Check-Valid-Until \"false\";' "
                "> /etc/apt/apt.conf.d/99snapshot" % (snapshot, snapshot))
        if any(":i386" in p for r in rows.values() for p in r[2]):
            box.sh("dpkg --add-architecture i386")
        box.sh("apt-get -o APT::Update::Error-Mode=any update -qq")

        local = {}          # row id -> (path in box, package name, KiB)
        for i in set(sum((chain(w) for w in want), [])):
            if i in debs:
                f = fetch_deb(*debs[i])
                box.cp_in(f, "/tmp/%s.deb" % i)
                name, kib = box.sh("dpkg-deb -f /tmp/%s.deb Package "
                                   "Installed-Size" % i).split("\n")[:2]
                local[i] = ("/tmp/%s.deb" % i, name.split(": ")[1],
                            int(kib.split(": ")[1]))

        sims = {}

        def sim(ids):
            key = tuple(ids)
            if key in sims:
                return sims[key]
            pk = []
            for i in ids:
                pk += rows[i][2]
                if i in local:
                    pk.append(local[i][0])
            got = {}
            if pk:
                out = box.sh("apt-get install -s --no-install-recommends %s"
                             % " ".join(pk))
                for line in out.splitlines():
                    m = INST.match(line)
                    if m:
                        n = m.group(1).split(":")[0]
                        arch = "amd64" if m.group(3) == "all" else m.group(3)
                        got["%s:%s" % (n, arch)] = m.group(2)
            sims[key] = got
            return got

        need = {}
        for w in want:
            c = chain(w)
            sim(c)
            sim(c[:-1])
        for s in sims.values():
            need.update(s)
        sizes = {}
        names = sorted(need)
        for k in range(0, len(names), 200):
            args = []
            for n in names[k:k + 200]:
                pkg, arch = n.split(":")
                args.append("%s%s=%s" % (pkg, ":i386" if arch == "i386" else "",
                                        need[n]))
            out = box.sh("apt-cache show --no-all-versions %s 2>/dev/null"
                         % " ".join("'%s'" % a for a in args), check=False)
            for st in out.split("\n\n"):
                f = dict(l.split(": ", 1) for l in st.splitlines()
                         if ": " in l and not l.startswith(" "))
                if "Package" in f and "Installed-Size" in f:
                    arch = f.get("Architecture", "amd64")
                    arch = "amd64" if arch == "all" else arch
                    sizes["%s:%s" % (f["Package"], arch)] = \
                        int(f["Installed-Size"]) * 1024
        for i, (_, name, kib) in local.items():
            sizes["%s:amd64" % name] = kib * 1024

        result = {}
        print("%-26s %14s %14s %6s" % ("ROW", "META", "MEASURED", "PKGS"))
        for w in want:
            c = chain(w)
            added = set(sim(c)) - set(sim(c[:-1]))
            missing = [a for a in added if a not in sizes]
            if missing:
                print("  %s: no Installed-Size for %s" % (w, " ".join(missing)),
                      file=sys.stderr)
            result[w] = sum(sizes.get(a, 0) for a in added)
            print("%-26s %14d %14d %6d" % (w, meta.get(w, 0), result[w],
                                            len(added)))
    finally:
        box.close()

    if o.write:
        text = open(o.catalogue, encoding="utf-8").read().split("\n")
        for n, line in enumerate(text):
            w = line.split()
            if len(w) >= 3 and w[0] == "meta" and w[1] in result:
                head, rest = line.split("|", 1)
                f = rest.split("|")
                f[1] = str(result[w[1]])
                text[n] = head + "|" + "|".join(f)
        open(o.catalogue, "w", encoding="utf-8").write("\n".join(text))


if __name__ == "__main__":
    main()
