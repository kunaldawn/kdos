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
CLEAN_PYCACHE=1         # __pycache__ / *.pyc / *.pyo

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
if [ "$CLEAN_PYCACHE" = 1 ]; then
    section "Python __pycache__ / *.pyc"
    before=$(find "$FS" \( -name __pycache__ -o -name "*.pyc" -o -name "*.pyo" \) \
              -printf "%s\n" 2>/dev/null | awk '{s+=$1} END {print s+0}')
    find "$FS" -name __pycache__ -type d -prune -exec rm -rf {} + 2>/dev/null || true
    find "$FS" \( -name "*.pyc" -o -name "*.pyo" \) -delete 2>/dev/null || true
    report "pycache + .pyc/.pyo" "$before" 0
fi

# ── The monolithic appbox's container store ─────────────────────────────
#
# THE IMAGE LANE IS GONE AND ITS STORE MUST GO WITH IT. `01_appbox.sh` used to
# `podman load` the ~4 GB kdos-apps image into this directory so a live session
# had it pre-seeded; nothing creates it now, and nothing REMOVED it either — so
# 10.4 GB of a deleted lane rode into system.sfs on every build, which is the
# same way 529 MB of a removed desktop rode three ISOs before the package sweep
# existed. The pack lane composes a box at first launch, in the user's own home
# on the running system, so a shipped rootfs carries no container store at all.
#
# Not a `kpkgdel`: no package owns these paths — podman wrote them.
if [ -d "$FS/home/kdos/.local/share/containers" ]; then
    section "the monolithic appbox's container store"
    before=$(size_of "$FS/home/kdos/.local/share/containers")
    rm -rf "$FS/home/kdos/.local/share/containers"
    report "home/kdos/.local/share/containers" "$before" 0
fi

# ── Done ────────────────────────────────────────────────────────────────
FINAL=$(du -sh "$FS" 2>/dev/null | cut -f1)
echo
echo "[KDOS] Cleanup done. $INITIAL → $FINAL"
