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
# THE DIRECTORY ACCUMULATES. Each package is MOVED out of build/fs's package
# cache, so the cache holds only what the next build makes: under
# KDOS_IMAGE=base, 020_cleanup.sh empties the cache of the image stage and
# never build/fs's. An incremental build makes only the packages it rebuilt,
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
# RUNS BEFORE 017_stage.sh, on build/fs, whose cache holds every package the
# build kept. The step number does the sequencing.

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
    mv -f "$p" "$OUT/"
    added=$(( added + 1 ))
done

total=$(find "$OUT" -maxdepth 1 -name '*.tar.xz' | wc -l)
if [ "$total" -eq 0 ]; then
    echo "FATAL: KDOS_MAKE_BINHOST=1 and no package was kept." >&2
    echo "       Packages are kept only by a build that installs them with the flag set." >&2
    exit 1
fi

echo "[binhost] $added package(s) from this build, $total in $OUT"
# EVERY REPOSITORY THE BUILD INSTALLS FROM, so a package of KDOS's own gets
# its recipe's hash as its E: like any other. A machine looks its recipes up
# under /ports/src/<area>, the same files 105_ports.sh copies there, and
# computes the same hash; with /ports/core alone every src/ package would be
# indexed with an empty E:, which no machine takes.
PORT_REPO="/ports/core /kdos/src/system /kdos/src/art /kdos/src/desktop /kdos/src/daemons" \
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

# THE RELEASE INDEX, over every file here but itself, the public key and
# every superseded package. A package stanza (one with no O:, which marks a
# delta) whose E: is empty is a file an earlier run left that matches no
# client: it stays in build/binhost and in PACKAGES, and is never published,
# so a release carries exactly its own tree's packages. A current package
# larger than one GitHub asset stops the build: `kdos update install` takes a
# package whole, and the release could not carry it.
source ports/releaselib.sh
source ports/srclib.sh
IDX=$OUT/$REL_INDEX_FILE
rm -f "$IDX" "$IDX.sig"
version=${KDOS_GIT_COMMIT:-local}
version=${version:0:12}
declare -A stale=()
while IFS= read -r f; do
    stale[$f]=1
done < <(awk -v RS= -F'\n' '{
    file = ""; e = "-"; delta = 0
    for (i = 1; i <= NF; i++) {
        if ($i ~ /^F:/) file = substr($i, 3)
        else if ($i ~ /^E:/) e = substr($i, 3)
        else if ($i ~ /^O:/) delta = 1
    }
    if (file != "" && !delta && e == "") print file
}' "$OUT/PACKAGES")
too_big=0
for f in "$OUT"/*.tar.xz; do
    [ -f "$f" ] && [ -z "${stale[${f##*/}]:-}" ] || continue
    if [ "$(stat -c %s "$f")" -gt "$SRC_PART_SIZE" ]; then
        echo "FATAL: ${f##*/} is larger than one release asset ($SRC_PART_SIZE bytes)" >&2
        too_big=1
    fi
done
[ "$too_big" = 0 ] || exit 1
echo "[binhost] ${#stale[@]} superseded package(s) kept here and left out of the release"
for f in "$OUT"/*; do
    [ -f "$f" ] || continue
    n=${f##*/}
    [ -z "${stale[$n]:-}" ] && [ -z "${stale[${n%.sig}]:-}" ] || continue
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
