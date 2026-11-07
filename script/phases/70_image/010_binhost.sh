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
# `kpkg binhost` reads (packaging.md, "The binary host"), and writes the
# release index `kdos update` finds every file of it by.
#
# EVERY NAME IS FLAT, AND THE RELEASE INDEX SAYS WHERE EACH FILE IS. A GitHub
# release holds no directories, so the AppStream catalogue is written as
# swcatalog--xml--kdos-binhost.xml beside the packages, and kdos-update.idx
# (ports/releaselib.sh writes it, relidx.c reads it) maps each file to the
# path a client asks for — `binhost/PACKAGES`, `binhost/<package>`,
# `binhost/swcatalog/xml/kdos-binhost.xml` — with its size and SHA-256, every
# asset beside the index. It is signed with the binhost key, so the directory,
# copied to a stick or served as it is, is what `binhost =` in
# /etc/kdos/update.conf names. `ports/publish --release` puts the same files in
# the releases binhost-<version>-NN and writes the release's own index over
# them. Its `version` is the short commit hash the Makefile records, cut to
# twelve characters, which is what a machine whose /ports carries no release
# name calls its tree.
#
# THE DIRECTORY ACCUMULATES. 020_cleanup.sh empties the package cache on every
# 70_image run, and an incremental build makes only the packages it rebuilt,
# so each run adds what it built and keeps what earlier runs added. A package
# older runs left is indexed beside its successor, and only the file of the
# version and release the ports tree's recipe makes gets that recipe's hash as
# its `E:`; a superseded one gets an empty `E:`, which no client's equality
# test accepts, so it is listed and never installed. Nothing here empties
# build/binhost; a binhost holding only this tree's packages needs its
# *.tar.xz removed before a build that installs every port with the flag set.
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

# THE CATALOGUE NAMES ONLY WHAT THE INDEX HOLDS AND THIS TREE HAS INSTALLED:
# Discover offers each of its components as a port `kdos update install` takes
# from this binhost. A package file in the directory whose package this tree
# does not have stays indexed and goes uncatalogued. It is signed with the
# index's key, the one key a client checks the two against.
CAT=$OUT/swcatalog--xml--kdos-binhost.xml
rm -f "$CAT" "$CAT.sig"
if ! command -v kdos-discover-catalog >/dev/null 2>&1; then
    echo "[binhost] kdos-discover-catalog is not installed — no catalogue"
else
    pkgs=()
    while IFS= read -r p; do
        [ -f "/var/lib/kpkg/db/$p" ] && pkgs+=(--package "$p")
    done < <(grep '^P:' "$OUT/PACKAGES" | cut -d: -f2- | LC_ALL=C sort -u)
    if [ "${#pkgs[@]}" -eq 0 ]; then
        echo "[binhost] no indexed package is installed in this tree — no catalogue"
    else
        kdos-discover-catalog --root / --origin kdos-binhost --out "$CAT" "${pkgs[@]}"
        kpkg sign "$CAT" "$KEYDIR/$KEY.key"
        echo "[binhost] $(grep -c '<component[ >]' "$CAT" || true) components in $CAT"
    fi
fi

# THE RELEASE INDEX, over every file here but itself and the public key.
source ports/releaselib.sh
IDX=$OUT/$REL_INDEX_FILE
rm -f "$IDX" "$IDX.sig"
version=${KDOS_GIT_COMMIT:-local}
version=${version:0:12}
for f in "$OUT"/*; do
    [ -f "$f" ] || continue
    n=${f##*/}
    case $n in
        "$REL_INDEX_FILE"|"$REL_INDEX_FILE".*|*.pub|*.tmp) continue ;;
        swcatalog--xml--kdos-binhost.xml)     path=binhost/swcatalog/xml/kdos-binhost.xml ;;
        swcatalog--xml--kdos-binhost.xml.sig) path=binhost/swcatalog/xml/kdos-binhost.xml.sig ;;
        *)                                    path=binhost/$n ;;
    esac
    rel_entry "$OUT" "$n" "$path"
done | rel_write "$IDX" "$version"
kpkg sign "$IDX" "$KEYDIR/$KEY.key"
KPKG_KEYRING="$KEYDIR" kpkg verify-pkg "$IDX"
echo "[binhost] $(rel_entries "$IDX" | wc -l) files in $IDX, version $version"
