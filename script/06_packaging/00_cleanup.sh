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
# Trim build/fs before packaging. Removes build-time-only artifacts that
# have no purpose in a runtime/live system, and slims the resulting ISO.
#
# Runs inside the chroot — operates on /kdos/build/fs (= chroot root via
# bind-mount).
#
# Each phase is gated by a flag below. Set to 0 to skip.

set -e
source script/packaging.env.sh

FS=/kdos/build/fs

# ── Phase gates ─────────────────────────────────────────────────────────
CLEAN_BUILD_CACHE=1     # /root/.cache, ~/.cargo, ~/.npm, /tmp, kpkg work dir
CLEAN_KPKG_PACKAGES=1   # /var/cache/kpkg/packages (built .tar.xz cache)
CLEAN_PYCACHE=1         # __pycache__ / *.pyc / *.pyo, then recompiled

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
    # bake used to leave multi-gigabyte podman<pid>/ directories in it that
    # rode all the way into the ISO.
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
if [ "$CLEAN_KPKG_PACKAGES" = 1 ]; then
    section "kpkg package cache"
    target="$FS/var/cache/kpkg/packages"
    if [ -d "$target" ]; then
        before=$(size_of "$target")
        find "$target" -maxdepth 1 -name "*.tar.*" -delete
        after=$(size_of "$target")
        report "var/cache/kpkg/packages/*.tar.*" "$before" "$after"
    fi
fi

# ── Python bytecode caches ─────────────────────────────────────────────
#
# DELETED, THEN COMPILED AGAIN — NEVER LEFT DELETED. What the build leaves is
# mixed: bytecode stamped with a build-time mtime, bytecode for files that no
# package owns, caches written by build tools running in the chroot. None of
# it can be reproduced, so it goes. But /usr is squashfs on the live medium
# and root's on a disk, so a Python program cannot write its cache back: with
# no bytecode, every start of every Python tool compiles everything it
# imports, every time.
#
# checked-hash, because it is the mode that is both reproducible and safe
# across updates. The .pyc records a hash of its source rather than an mtime,
# so the same tree compiles to the same bytes; and the interpreter checks that
# hash on import, so a source file a later `kpkg` upgrade replaces is
# recompiled in memory rather than served stale. unchecked-hash would skip
# that check and run the old bytecode.
#
# compileall's status is not the assertion: one file of Python 2 syntax in a
# site-packages tree fails its compile and sets it, while every other file is
# written.
if [ "$CLEAN_PYCACHE" = 1 ]; then
    section "Python __pycache__ / *.pyc"
    before=$(find "$FS" \( -name __pycache__ -o -name "*.pyc" -o -name "*.pyo" \) \
              -printf "%s\n" 2>/dev/null | awk '{s+=$1} END {print s+0}')
    find "$FS" -name __pycache__ -type d -prune -exec rm -rf {} + 2>/dev/null || true
    find "$FS" \( -name "*.pyc" -o -name "*.pyo" \) -delete 2>/dev/null || true
    report "pycache + .pyc/.pyo" "$before" 0

    if command -v python3 >/dev/null 2>&1; then
        for _py in /usr/lib/python3*; do
            [ -d "$_py" ] || continue
            python3 -m compileall -q --invalidation-mode checked-hash "$_py" \
                >/dev/null || echo "  compileall: some files under $_py did not compile" >&2
        done
        after=$(find "$FS/usr/lib" -path "*/__pycache__/*.pyc" \
                  -printf "%s\n" 2>/dev/null | awk '{s+=$1} END {print s+0}')
        [ "$after" -gt 0 ] || { echo "FATAL: no bytecode was compiled under /usr/lib/python3*" >&2; exit 1; }
        printf "  %-40s wrote %s\n" "checked-hash bytecode" "$(human $after)"
    fi
fi

# ── podman's container store ────────────────────────────────────────────
#
# A SHIPPED ROOTFS CARRIES NO CONTAINER STORE. Applications are built on the
# machine that asks for one, in that user's own home, so anything here is
# somebody else's images riding into system.sfs — measured at 10.4 GB when a
# build left one behind, which is the same way 529 MB of a removed desktop rode
# three ISOs before the package sweep existed.
#
# Not a `kpkgdel`: no package owns these paths — podman wrote them.
if [ -d "$FS/home/kdos/.local/share/containers" ]; then
    section "podman's container store"
    before=$(size_of "$FS/home/kdos/.local/share/containers")
    rm -rf "$FS/home/kdos/.local/share/containers"
    report "home/kdos/.local/share/containers" "$before" 0
fi

# ── Done ────────────────────────────────────────────────────────────────
FINAL=$(du -sh "$FS" 2>/dev/null | cut -f1)
echo
echo "[KDOS] Cleanup done. $INITIAL → $FINAL"
