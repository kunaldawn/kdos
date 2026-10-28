#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------
#
# Trim build/fs before the image is assembled. Removes build-time-only artifacts that
# have no purpose in a runtime/live system, and slims the resulting ISO.
#
# Runs inside the chroot — operates on /kdos/build/fs (= chroot root via
# bind-mount).
#
# Each phase is gated by a flag below. Set to 0 to skip.

set -e
source script/phases/70_image/phase.env

FS=/kdos/build/fs

# ── Phase gates ─────────────────────────────────────────────────────────
CLEAN_BUILD_CACHE=1     # /tmp, /var/tmp (holds the build's HOME), /root's caches, kpkg work dir
CLEAN_KPKG_PACKAGES=1   # /var/cache/kpkg/packages (built .tar.xz cache, .pending records)
CHECK_PYCACHE=1         # unowned __pycache__ / *.pyc / *.pyo removed; owned .py checked
CLEAN_UNOWNED_DOCS=1    # unowned catalogs, translated manuals, HTML, and the bootstrap's extras

# ── Helpers ─────────────────────────────────────────────────────────────
section() { echo; echo "── $* ──"; }
size_of() { du -sb "$1" 2>/dev/null | cut -f1; }
human() { numfmt --to=iec --suffix=B "${1:-0}" 2>/dev/null || echo "${1}B"; }
report() {
    local label="$1" before="$2" after="$3"
    local freed=$(( before - after ))
    printf "  %-40s freed %s\n" "$label" "$(human $freed)"
}

[ -d "$FS" ] || { echo "[CLEANUP] No $FS — nothing to do."; exit 0; }

INITIAL=$(du -sh "$FS" 2>/dev/null | cut -f1)
echo "[KDOS] Cleanup pass over $FS (initial size: $INITIAL)"

# ── Build cache ─────────────────────────────────────────────────────────
if [ "$CLEAN_BUILD_CACHE" = 1 ]; then
    section "Build cache"
    # var/tmp/* belongs here: it is scratch by definition, and the appbox
    # bake leaves podman<pid>/ directories there, multi-gigabyte, which would
    # otherwise ride into the ISO.
    for p in root/.cache root/.cargo root/.npm tmp/* var/tmp/* var/cache/kpkg/work; do
        target="$FS/$p"
        if compgen -G "$target" >/dev/null 2>&1; then
            before=$(size_of "$target")
            rm -rf $target  # unquoted to honour glob
            report "$p" "${before:-0}" 0
        fi
    done
fi

# ── Kpkg package cache (on-disk built tarballs) ─────────────────────────
#
# A `.pending` record names a package in this directory, so the two go
# together: a record left without its package reads as a build to commit that
# cannot be found.
if [ "$CLEAN_KPKG_PACKAGES" = 1 ]; then
    section "kpkg package cache"
    target="$FS/var/cache/kpkg/packages"
    if [ -d "$target" ]; then
        before=$(size_of "$target")
        find "$target" -maxdepth 1 \( -name "*.tar.*" -o -name "*.pending*" \) -delete
        after=$(size_of "$target")
        report "var/cache/kpkg/packages/{*.tar.*,*.pending*}" "$before" "$after"
    fi
fi

# ── Python bytecode ────────────────────────────────────────────────────
#
# BYTECODE SHIPS IN THE PACKAGES. kpkg compiles every Python tree a package
# ships into the package itself, as checked-hash bytecode (lint.c,
# kp_pkg_python), so the database owns each .pyc and an upgrade or removal
# handles it. /usr is squashfs on the live medium and root's on a disk, so a
# module without its .pyc is compiled on every import of every start, and the
# cache is never written.
#
# This step does two things to that:
#
#   - removes every .pyc, .pyo and __pycache__ that no package owns: bytecode
#     written by build tools importing modules in the chroot, stamped with a
#     build-time mtime, which nothing can reproduce or remove later;
#   - fails the image when a .py a package owns, outside /usr/bin and
#     /usr/sbin (scripts, run and never imported), has no .pyc for this
#     interpreter. `*.tmpl.py` is a template, not a module. gcc's libstdc++
#     pretty printers (usr/share/gcc-*/python) and gdb's auto-load scripts
#     are exempt: gcc is built in 20_selfhost and 30_foundation, before
#     python3 is installed, so kpkg has no interpreter to compile them, and
#     gdb reads them only in a debugging session. A file the interpreter
#     cannot compile (Python 2, a template under another name) is listed and
#     passes, because there is no bytecode to have.
#
# Ownership is read from the database with the merged-/usr aliases folded:
# a package may list `./lib/x` for what is on disk at usr/lib/x.
if [ "$CHECK_PYCACHE" = 1 ]; then
    section "Python bytecode"
    if command -v python3 >/dev/null 2>&1; then
        python3 -I - "$FS" <<'PY'
import importlib.util, os, sys

fs = sys.argv[1]
dbdir = os.path.join(fs, "var/lib/kpkg/db")

def canon(p):
    p = p[2:] if p.startswith("./") else p.lstrip("/")
    p = p.rstrip("/")
    for a in ("bin", "sbin", "lib"):
        if p == a or p.startswith(a + "/"):
            return "usr/" + p
    return p

owned = set()
for n in os.listdir(dbdir):
    f = os.path.join(dbdir, n)
    if n.startswith(".") or not os.path.isfile(f):
        continue
    with open(f, encoding="utf-8", errors="surrogateescape") as h:
        for line in h:
            if line.startswith("./"):
                owned.add(canon(line.rstrip("\n")))

removed = size = 0
caches = []
for top in ("usr", "opt", "etc", "var", "srv"):
    for d, dirs, files in os.walk(os.path.join(fs, top)):
        rd = os.path.relpath(d, fs)
        if os.path.basename(d) == "__pycache__":
            caches.append((d, rd))
        for f in files:
            if not f.endswith((".pyc", ".pyo")):
                continue
            rel = os.path.join(rd, f)
            if rel in owned:
                continue
            p = os.path.join(d, f)
            size += os.lstat(p).st_size
            os.unlink(p)
            removed += 1
for d, rd in reversed(caches):
    if rd not in owned and not os.listdir(d):
        os.rmdir(d)
print(f"  removed {removed} unowned bytecode file(s), {size // 1024} KiB")

missing, foreign = [], []
for rel in sorted(owned):
    if not rel.endswith(".py") or rel.endswith(".tmpl.py"):
        continue
    if rel.startswith("usr/share/gdb/auto-load/") or (
            rel.startswith("usr/share/gcc-") and "/python/" in rel):
        continue
    if not (rel.startswith("usr/") or rel.startswith("opt/")):
        continue
    if rel.startswith(("usr/bin/", "usr/sbin/")):
        continue
    p = os.path.join(fs, rel)
    if os.path.islink(p) or not os.path.isfile(p):
        continue
    if os.path.exists(importlib.util.cache_from_source(p)):
        continue
    try:
        with open(p, "rb") as h:
            compile(h.read(), p, "exec", dont_inherit=True)
    except (SyntaxError, ValueError):
        foreign.append(rel)
        continue
    missing.append(rel)

for rel in foreign:
    print(f"  not Python 3 source, no bytecode: /{rel}")
if missing:
    for rel in missing:
        print(f"  no bytecode: /{rel}", file=sys.stderr)
    print(f"FATAL: {len(missing)} packaged .py file(s) have no .pyc; "
          "the package that owns each was built by a kpkg that does not "
          "compile bytecode, or compiled it under another interpreter",
          file=sys.stderr)
    sys.exit(1)
print("  every packaged module has checked-hash bytecode")
PY
    fi
fi

# ── Unowned documentation and catalogs ──────────────────────────────────
#
# THE SIZE AND LANGUAGE POLICY COVERS FILES NO PACKAGE OWNS. kpkg prunes
# message catalogs, translated manual pages and HTML manuals from every
# package (prune.c), but 10_bootstrap installs its tools outside any package,
# and a port installed over them adopts only the paths it ships: a bootstrap
# copy of something its port prunes is adopted by nothing and would ride
# into the image. This step removes, when no package owns it and fs/ does
# not provide it:
#
#   - a non-English message catalog under usr/share/locale/<lang>/LC_MESSAGES;
#   - anything under a usr/share/man directory that is not man<section>;
#   - an .html, .htm or .pdf under usr/share/doc;
#   - the files the bootstrap's own ports leave out of their packages:
#     gawk's gawknotes.info, and binutils' dlltool, windres and windmc pages.
#
# The 10_bootstrap steps install none of these themselves; the sweep removes
# any the tree carries, whatever wrote it. 087_gates.sh then fails the
# image on any unowned file left under usr/share/{locale,man,doc,info}.
if [ "$CLEAN_UNOWNED_DOCS" = 1 ]; then
    section "Unowned documentation and catalogs"
    python3 -I - "$FS" <<'PY'
import os, sys

fs = sys.argv[1]
dbdir = os.path.join(fs, "var/lib/kpkg/db")

def canon(p):
    p = p[2:] if p.startswith("./") else p.lstrip("/")
    p = p.rstrip("/")
    for a in ("bin", "sbin", "lib"):
        if p == a or p.startswith(a + "/"):
            return "usr/" + p
    return p

owned = set()
for n in os.listdir(dbdir):
    f = os.path.join(dbdir, n)
    if n.startswith(".") or not os.path.isfile(f):
        continue
    with open(f, encoding="utf-8", errors="surrogateescape") as h:
        for line in h:
            if line.startswith("./"):
                owned.add(canon(line.rstrip("\n")))
try:
    with open(os.path.join(fs, "var/lib/kdos/fs-manifest"), encoding="utf-8",
              errors="surrogateescape") as h:
        owned.update(canon(ln.strip()) for ln in h if ln.strip())
except OSError:
    pass

EXTRA = {
    "usr/share/info/gawknotes.info",
    "usr/share/man/man1/dlltool.1",
    "usr/share/man/man1/windres.1",
    "usr/share/man/man1/windmc.1",
}

def english(tag):
    return tag == "en" or tag.startswith(("en_", "en-", "en@"))

def policy(rel):
    if rel in EXTRA:
        return True
    parts = rel.split("/")
    if rel.startswith("usr/share/locale/"):
        return (len(parts) == 6 and parts[4] == "LC_MESSAGES"
                and parts[5].endswith((".mo", ".gmo", ".qm"))
                and not english(parts[3]))
    if rel.startswith("usr/share/man/"):
        return len(parts) > 4 and not parts[3].startswith("man")
    if rel.startswith("usr/share/doc/"):
        return rel.endswith((".html", ".htm", ".pdf"))
    return False

removed = size = 0
emptied = set()
for top in ("usr/share/locale", "usr/share/man", "usr/share/doc", "usr/share/info"):
    base = os.path.join(fs, top)
    for d, dirs, files in os.walk(base, topdown=False):
        for f in files:
            p = os.path.join(d, f)
            rel = os.path.relpath(p, fs)
            if rel in owned or not policy(rel):
                continue
            size += os.lstat(p).st_size
            os.unlink(p)
            removed += 1
            emptied.add(d)
        if (d in emptied and d != base and os.path.relpath(d, fs) not in owned
                and not os.listdir(d)):
            os.rmdir(d)
            emptied.add(os.path.dirname(d))
print(f"  removed {removed} unowned file(s), {size // 1024} KiB")
PY
fi

# ── podman's container store ────────────────────────────────────────────
#
# A SHIPPED ROOTFS CARRIES NO CONTAINER STORE. Applications are built on the
# machine that asks for one, in that user's own home, so anything here is
# somebody else's images riding into system.sfs. A store runs to gigabytes, and
# nothing else in this phase removes it.
#
# Not a `kpkgdel`: no package owns these paths — podman wrote them.
if [ -d "$FS/home/kdos/.local/share/containers" ]; then
    section "podman's container store"
    before=$(size_of "$FS/home/kdos/.local/share/containers")
    rm -rf "$FS/home/kdos/.local/share/containers"
    report "home/kdos/.local/share/containers" "$before" 0
fi

# The rootful store goes for the same reason: an image or layer a build pulled
# as root is not something the shipped system asked for.
if [ -d "$FS/var/lib/containers/storage" ]; then
    section "podman's rootful store"
    before=$(size_of "$FS/var/lib/containers/storage")
    rm -rf "$FS/var/lib/containers/storage"
    report "var/lib/containers/storage" "$before" 0
fi

# ── root's home ─────────────────────────────────────────────────────────
#
# /root SHIPS EXACTLY WHAT fs/root HOLDS, MODE 0700. The build's HOME is a
# scratch directory (script/chroot/exec.sh), but a tool that names /root itself
# still writes there: a CMake package registry pointing into kpkg's work
# directories, which root's find_package would consult, git's safe.directory,
# dated telemetry and logs that differ build to build. Each would reach every
# installed system through the installer's copy.
section "root's home"
before=$(size_of "$FS/root")
rm -rf "$FS/root"
mkdir -p "$FS/root"
cp -R /kdos/fs/root/. "$FS/root/"
chmod -R go-w "$FS/root"
chmod 0700 "$FS/root"
report "root (reset to fs/root)" "$before" "$(size_of "$FS/root")"

# ── Done ────────────────────────────────────────────────────────────────
FINAL=$(du -sh "$FS" 2>/dev/null | cut -f1)
echo
echo "[KDOS] Cleanup done. $INITIAL → $FINAL"
