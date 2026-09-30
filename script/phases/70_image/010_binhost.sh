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
# Write the packages this build made into a signed binhost, when asked.
#
# `make build KDOS_MAKE_BINHOST=1` makes script/chroot/exec.sh export
# KPKG_KEEP_CACHE=1, so every `kpkg install` of the build leaves the package it
# built in /var/cache/kpkg/packages. This step copies them to build/binhost/
# and runs `kpkg index --sign` over that directory, which is then a binhost
# `kpkg binhost` and `kdos update apply` read (packaging.md, "The binary
# host"). Off, it does nothing.
#
# THE DIRECTORY ACCUMULATES. 020_cleanup.sh empties the package cache on every
# 70_image run, and an incremental build makes only the packages it rebuilt,
# so each run adds what it built and keeps what earlier runs added. A package
# older runs left is indexed beside its successor and matches no machine whose
# recipe moved on, because the index's `E:` is the recipe hash. A complete
# binhost therefore needs one `--fresh` build with the flag set.
#
# THE SIGNING KEY IS build/binhost-key/kdos-binhost.key, made on the first run
# and never replaced; it stays under build/ and never enters the image. A
# machine trusts the binhost by holding kdos-binhost.pub, which is copied
# beside the index, in /etc/kdos/keys.
#
# RUNS BEFORE 020_cleanup.sh, which deletes the package cache. The step
# number does the sequencing.

set -e
source script/phases/70_image/phase.env

if [ "${KDOS_MAKE_BINHOST:-0}" != 1 ]; then
    echo "[binhost] off — make build KDOS_MAKE_BINHOST=1 writes one"
    exit 0
fi

CACHE=/var/cache/kpkg/packages
OUT=/kdos/build/binhost
KEYDIR=/kdos/build/binhost-key
KEY=kdos-binhost

mkdir -p "$OUT" "$KEYDIR"
if [ ! -f "$KEYDIR/$KEY.key" ]; then
    echo "[binhost] making the signing key $KEYDIR/$KEY.key"
    ( cd "$KEYDIR" && kpkg keygen "$KEY" )
fi
cp -f "$KEYDIR/$KEY.pub" "$OUT/$KEY.pub"

added=0
for p in "$CACHE"/*.tar.xz; do
    [ -f "$p" ] || continue
    cp -f "$p" "$OUT/"
    added=$(( added + 1 ))
done

total=$(find "$OUT" -maxdepth 1 -name '*.tar.xz' | wc -l)
if [ "$total" -eq 0 ]; then
    echo "FATAL: KDOS_MAKE_BINHOST=1 and no package was kept." >&2
    echo "       Packages are kept only by a build that installs them with the flag set." >&2
    exit 1
fi

echo "[binhost] $added package(s) from this build, $total in $OUT"
kpkg index "$OUT" --sign "$KEYDIR/$KEY.key"
# Verified against the one key it was signed with, the way a client that
# trusts it will read it.
KPKG_KEYRING="$KEYDIR" kpkg verify-index "$OUT"
