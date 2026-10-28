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
# The recipe tree every image carries at /ports, and the source offer at
# /usr/share/kdos/SOURCES.
#
# /ports IS RECIPES AND NOTHING ELSE: each port's kpkgbuild, build.sh, patches
# and other recipe files, ports/shelves, ports/sources.idx, and src/, the two
# books under docs/ and script/book for the programs of our own, whose sources
# no archive holds. A shipped machine updates
# by building from this tree, and kpkg fetches each source over the network
# from the sources-<N> archive or upstream. The fetched tarballs are tens of
# gigabytes and stay off the medium.
#
# A FILE IS A SOURCE WHEN ITS RECIPE HASHES IT (`sha256 = <hash>  <file>`) OR
# WHEN IT IS AN ARCHIVE. The second rule catches a tarball an older version of
# the recipe named and no recipe names now; without it the tree carries every
# stale download left in a port directory.
#
# The tree is STAGED in build/image-ports and appended to system.sfs by
# 110_iso.sh. /ports in the chroot is the build's bind of the repository, so it
# is excluded from the first squash and cannot be filtered in place.
#
# src/ goes to /ports/src, keeping its areas side by side: a desktop program's
# build.sh reaches the libraries as $PORT_SRC/../../libs. kpkg.conf names the
# areas that hold ports in PORT_REPO. docs/ goes to /ports/docs beside it,
# and script/book to /ports/script/book: kdos-doc reaches its book through
# its `kdos` and `screenshots` links (../../../docs/kdos and
# ../../../docs/screenshots), and kdos-kfs reaches KDOS from Scratch and its
# renderer through its `docs` and `book` links (../../../docs/kfs and
# ../../../script/book). The recipe hash follows those links, and one left
# dangling drops out of it, so /ports would hash the port differently from
# the tree its package was built from and the machine would take the
# installed package for stale. kdos-kfs builds only in a checkout even so:
# its renderer reads ports/core and script/phases, which /ports does not have.

set -e
source script/phases/70_image/phase.env

umask 022

STAGE=/kdos/build/image-ports
TREE=$STAGE/ports
OFFER=/usr/share/kdos/SOURCES
ARCHIVE=https://github.com/kunaldawn/kdos

echo "Staging the recipe tree for /ports..."
rm -rf "$STAGE"
mkdir -p "$TREE/core"

for f in shelves sources.idx; do
    [ -f "/ports/$f" ] || { echo "FATAL: /ports/$f is missing" >&2; exit 1; }
    cp "/ports/$f" "$TREE/$f"
done

nports=0
nfiles=0
for k in /ports/core/*/*/kpkgbuild; do
    d=${k%/kpkgbuild}
    rel=${d#/ports/}
    unset hashed
    declare -A hashed=()
    while IFS= read -r f; do
        [ -n "$f" ] && hashed[$f]=1
    done < <(awk '$1 == "sha256" { print $4 }' "$k")
    mkdir -p "$TREE/$rel"
    while IFS= read -r -d '' f; do
        f=${f#"$d"/}
        [ -n "${hashed[$f]:-}" ] && continue
        case "$f" in
            *.tar|*.tar.*|*.tgz|*.tbz2|*.txz|*.zip|*.7z|*.part|*.jar) continue ;;
        esac
        case "$f" in */*) mkdir -p "$TREE/$rel/${f%/*}" ;; esac
        cp "$d/$f" "$TREE/$rel/$f"
        nfiles=$((nfiles + 1))
    done < <(find "$d" -mindepth 1 -name '.*' -prune -o -type f -print0)
    nports=$((nports + 1))
done
[ "$nports" -gt 0 ] || { echo "FATAL: no recipe under /ports/core" >&2; exit 1; }

cp -R /kdos/src "$TREE/src"
for d in docs/kdos docs/screenshots docs/kfs script/book; do
    [ -d "/kdos/$d" ] || { echo "FATAL: /kdos/$d is missing; kdos-doc and kdos-kfs link to it" >&2; exit 1; }
    mkdir -p "$TREE/${d%/*}"
    cp -R "/kdos/$d" "$TREE/$d"
done
find "$TREE/src" "$TREE/docs" "$TREE/script" -mindepth 1 \( -name '.*' -o -name __pycache__ \) -prune -exec rm -rf {} +
chmod -R u=rwX,go=rX "$STAGE"
nsrc=$(find "$TREE/src" -mindepth 3 -maxdepth 3 -name kpkgbuild | wc -l)

# The tree must hold no source: one archive is the first of tens of gigabytes.
leak=$(find "$TREE/core" -type f -size +16M | head -n 5)
if [ -n "$leak" ]; then
    echo "FATAL: the recipe tree holds files over 16 MB, which are sources:" >&2
    echo "$leak" >&2
    exit 1
fi

echo "  $nports ports ($nfiles files), $nsrc of our own under src/, $(du -sh "$STAGE" | cut -f1)"

# ── The tree's stamp and the key for the next one ───────────────────────
#
# TREE names the commit this tree is and when it was made. `kdos update fetch`
# treats a tree naming the same commit as current, refuses one whose time is
# older, and replaces /ports with any other signed tree, so an image without
# the stamp takes any signed tree, an older one included. The format
# is the one `ports/publish --release` writes into the published tree.
#
# THE RELEASE KEY'S PUBLIC HALF GOES INTO /etc/kdos/keys/tree, the ring
# `kdos update fetch` verifies a published tree against. It is the key
# `ports/publish --release` signs with, kept in build/keys/ and never in git.
# A build without it ships an empty ring, and that image refuses every
# fetched tree rather than trusting an unsigned one.
if [ -n "${KDOS_GIT_COMMIT:-}" ]; then
    {
        echo "kdos-tree 1"
        echo "commit $KDOS_GIT_COMMIT"
        echo "time ${KDOS_GIT_TIME:-0}"
    } > "$TREE/TREE"
    chmod 0644 "$TREE/TREE"
else
    echo "  no commit given: /ports carries no TREE stamp"
fi
RELEASE_PUB=/kdos/build/keys/kdos-release.pub
TREE_RING=/etc/kdos/keys/tree
mkdir -p "$TREE_RING"
rm -f "$TREE_RING"/*.pub
if [ -f "$RELEASE_PUB" ]; then
    install -m 0644 "$RELEASE_PUB" "$TREE_RING/kdos-release.pub"
    echo "  $TREE_RING trusts build/keys/kdos-release.pub"
else
    echo "  no build/keys/kdos-release.pub: $TREE_RING is empty and this image fetches no tree"
fi

# ── The source offer ────────────────────────────────────────────────────
#
# The commit names every recipe and every program of our own; the releases
# name where each upstream source an installed package was built from is
# archived. A release is listed when sources.idx places one of those hashes in
# it. A hash the index does not hold is reported here and fetched from the
# upstream URL its recipe names.
echo "Writing $OFFER..."
recipe_of() {
    local n=$1 r
    for r in /ports/core/*/"$n"/kpkgbuild /kdos/src/*/"$n"/kpkgbuild; do
        [ -f "$r" ] && { echo "$r"; return 0; }
    done
    return 1
}

hashes=$(mktemp)
for p in /var/lib/kpkg/db/*; do
    [ -f "$p" ] || continue
    r=$(recipe_of "${p##*/}") || continue
    awk '$1 == "sha256" { print $3 }' "$r"
done | sort -u > "$hashes"

releases=$(awk 'NR == FNR { want[$1] = 1; next }
                !/^#/ && ($1 in want) { print $2 }' "$hashes" /ports/sources.idx \
           | sort -u -V)
unarchived=$(awk 'NR == FNR { if (!/^#/) have[$1] = 1; next }
                  !($1 in have)' /ports/sources.idx "$hashes" | wc -l)
rm -f "$hashes"

commit="${KDOS_GIT_COMMIT:-unrecorded}"
[ -n "${KDOS_GIT_COMMIT:-}" ] && [ "${KDOS_GIT_DIRTY:-0}" = 1 ] && commit="$commit-dirty"

mkdir -p "${OFFER%/*}"
{
    echo "# The source of everything on this image."
    echo "#"
    echo "# The recipes and the programs of our own are the KDOS tree at this commit,"
    echo "# and a copy of them is /ports. Each upstream source a package was built"
    echo "# from is archived, by its sha256, in the releases below; /ports/sources.idx"
    echo "# says which release holds which file."
    echo
    echo "repository  $ARCHIVE"
    echo "commit      $commit"
    [ -n "${KDOS_GIT_DATE:-}" ] && echo "date        $KDOS_GIT_DATE"
    for t in $releases; do
        echo "release     $ARCHIVE/releases/tag/$t"
    done
} > "$OFFER"
chmod 0644 "$OFFER"

[ -n "$releases" ] || { echo "FATAL: no installed package's source is in sources.idx" >&2; exit 1; }
echo "  commit $commit; releases: $(echo $releases)"
if [ "$unarchived" -gt 0 ]; then
    echo "  warning: $unarchived source(s) of installed packages are not in sources.idx;" >&2
    echo "           ports/publish has not run for them" >&2
fi
