# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   ports/srclib.sh — the source archive's addressing, shared
# ---------------------------------
#
# Sourced by ports/fetch, ports/publish and script/hooks/pre-push. Nothing here
# downloads or uploads; it answers "where does this hash live" and "which
# hashes does this port name", so the three callers cannot disagree about
# either.
#
# THE ARCHIVE IS CONTENT-ADDRESSED. An archived file is the release asset
#
#     https://github.com/kunaldawn/kdos/releases/download/sha256-<h:0:2>/<h>
#
# where <h> is the 64-hex `sha256 =` the recipe already carries. The recipe
# hash, the asset name and the digest GitHub computes for the asset are the
# same string, so a URL needs no index, no manifest and no lookup, and a file
# that verifies is the file the recipe meant whatever path it came by.
#
# The name is the bare hash because GitHub rewrites asset names containing
# anything outside [A-Za-z0-9._-] — a `+` in an upstream filename would
# otherwise produce a URL nothing requests — and because two upstream
# releases of different bytes under one filename cannot collide on a hash.
#
# 256 RELEASES, ONE PER LEADING BYTE, because a release holds at most 1000
# assets. Hashes spread evenly, so a shard reaches that cap only somewhere past
# 200,000 archives; a filename-keyed shard skews by first letter (`lib*`,
# `python-*`) and fills within a couple of years.
#
# APPEND-ONLY. An asset is never replaced or deleted once its digest matches
# its name: a five-year-old checkout finds the exact bytes it was written
# against because nothing was allowed to take them away.
#
# THE SHARDS ARE RELEASES OF THE MAIN REPOSITORY, so its release page lists
# them beside the KDOS releases (make_latest false keeps "latest" on a KDOS
# release). GitHub's immutable releases must stay OFF on it: the setting is
# repository-wide, and it freezes a shard at its first publication, after
# which no new source can ever be added to it.

KDOS_SOURCES_REPO="${KDOS_SOURCES_REPO:-kunaldawn/kdos}"
# Empty means upstream only — no archive is consulted.
KDOS_SOURCES_BASE="${KDOS_SOURCES_BASE-https://github.com/$KDOS_SOURCES_REPO/releases/download}"

SRCLIB_PORTS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRCLIB_ROOT="$(dirname "$SRCLIB_PORTS")"

# The local cache, laid out exactly like the archive, gitignored, with each
# port directory holding a hard link into it. It belongs to this checkout, so
# a branch switch downloads nothing; a second checkout or a re-clone downloads
# nothing only when KDOS_SRCCACHE names a cache shared between them.
KDOS_SRCCACHE="${KDOS_SRCCACHE:-$SRCLIB_PORTS/.srccache}"
# Absolute from here on: a relative cache would follow whatever directory a
# caller later changes into and split into one cache per directory.
case $KDOS_SRCCACHE in
    /*) ;;
    *) KDOS_SRCCACHE="$PWD/$KDOS_SRCCACHE" ;;
esac

# src_is_hash <word> — true for exactly 64 lowercase hex digits.
src_is_hash() {
    [[ "$1" =~ ^[0-9a-f]{64}$ ]]
}

# src_shard <hash> — the release tag holding it.
src_shard() {
    printf 'sha256-%s\n' "${1:0:2}"
}

# src_url <hash> — where the archive serves it. Empty when the archive is off.
src_url() {
    [ -n "$KDOS_SOURCES_BASE" ] || return 1
    printf '%s/%s/%s\n' "$KDOS_SOURCES_BASE" "$(src_shard "$1")" "$1"
}

# src_cache_path <hash>
src_cache_path() {
    printf '%s/%s/%s\n' "$KDOS_SRCCACHE" "$(src_shard "$1")" "$1"
}

# src_hash_ok <file> <hash> — the file exists, is not empty, and hashes to
# <hash>. An empty file is refused before hashing: an interrupted download
# leaves one, and its digest is as stable as any other.
src_hash_ok() {
    [ -s "$1" ] || return 1
    [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ]
}

# src_cache_put <file> <hash> — enter a verified file into the cache by hard
# link, falling back to a copy across filesystems. The caller has verified it.
# An entry that is already this file is kept; any other entry is kept only if
# it verifies, because a corrupt one left in place is served to the next
# checkout that asks for the hash.
src_cache_put() {
    local dst; dst=$(src_cache_path "$2")
    [ "$1" -ef "$dst" ] && return 0
    src_hash_ok "$dst" "$2" && return 0
    mkdir -p "$(dirname "$dst")"
    ln -f "$1" "$dst" 2>/dev/null || cp -f "$1" "$dst"
}

# src_cache_get <hash> <dest> — materialise a cached file at <dest>, verified.
# A cache entry that no longer hashes to its name is removed, not trusted.
src_cache_get() {
    local src; src=$(src_cache_path "$1")
    [ -s "$src" ] || return 1
    if ! src_hash_ok "$src" "$1"; then
        rm -f "$src"
        return 1
    fi
    rm -f "$2"
    ln "$src" "$2" 2>/dev/null || cp -f "$src" "$2"
}

# THE RECIPE READER. kdos-kpkg parses kpkgbuild files; `kpkg meta` prints the
# fields as shell assignments. It is compiled here, on the host, from the tree,
# and REBUILT WHENEVER ITS SOURCES ARE NEWER THAN THE BINARY: a reader built
# before a parser change keeps reading recipes the old way, and a truncated
# `source =` looks exactly like a recipe that names fewer files.
#
# kdos-kpkg's primary dispatch is argv[0]'s basename, so the binary must be
# named literally `kpkg`. It links libkbase, libkpkg and libksig
# (kdos-kpkg.h includes ksig.h); src/tools/kdos-portup/main.c and
# testing/selftest.sh compile the same set and all three must agree.
KPKG="${KPKG_BIN:-$SRCLIB_PORTS/.kpkgbin/kpkg}"

src_kpkg_ensure() {
    local srcs=(
        "$SRCLIB_ROOT"/src/packages/kdos-kpkg/*.[ch]
        "$SRCLIB_ROOT"/src/libs/libkbase/*.[ch]
        "$SRCLIB_ROOT"/src/libs/libkpkg/*.[ch]
        "$SRCLIB_ROOT"/src/libs/libksig/*.[ch]
        "$SRCLIB_ROOT"/src/libs/libksig/monocypher/*.[ch]
    )
    if [ -x "$KPKG" ]; then
        local f stale=0
        for f in "${srcs[@]}"; do
            [ "$f" -nt "$KPKG" ] && { stale=1; break; }
        done
        [ "$stale" = 0 ] && return 0
    fi
    echo "==> Building the recipe reader..." >&2
    mkdir -p "$(dirname "$KPKG")"
    ${CC:-cc} -O2 -std=gnu11 -D_GNU_SOURCE \
        -I"$SRCLIB_ROOT/src/libs/libkbase" -I"$SRCLIB_ROOT/src/libs/libkpkg" \
        -I"$SRCLIB_ROOT/src/libs/libksig" \
        -I"$SRCLIB_ROOT/src/packages/kdos-kpkg" -o "$KPKG.tmp" \
        "$SRCLIB_ROOT"/src/packages/kdos-kpkg/*.c \
        "$SRCLIB_ROOT"/src/libs/libkbase/*.c "$SRCLIB_ROOT"/src/libs/libkpkg/*.c \
        "$SRCLIB_ROOT"/src/libs/libksig/*.c "$SRCLIB_ROOT"/src/libs/libksig/monocypher/*.c \
        && mv -f "$KPKG.tmp" "$KPKG"
}

# src_port_hashes <portdir> — one line per `sha256 =` entry: "<hash> <file>".
# `kpkg meta` emits $sha256 as alternating "<hex> <name>" tokens.
src_port_hashes() {
    local meta sha256=""
    meta=$(cd "$1" && "$KPKG" meta . 2>/dev/null) || return 1
    sha256=$(unset sha256; eval "$meta"; printf '%s' "${sha256:-}")
    printf '%s\n' $sha256 | awk 'NR%2==1{h=$0} NR%2==0{print h" "$0}'
}

# src_tracked <path> — git already carries this file (a patch, a config file,
# a certificate bundle), so the archive neither needs nor holds it.
src_tracked() {
    git -C "$SRCLIB_ROOT" ls-files --error-unmatch -- "$1" >/dev/null 2>&1
}
