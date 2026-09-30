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
# Sourced by ports/fetch, ports/publish, script/hooks/pre-push and
# testing/preflight.sh. Nothing here downloads or uploads; it answers "where
# does this hash live", "which hashes does this port name" and "is this index
# well-formed", so the callers cannot disagree about any of them.
#
# THE ARCHIVE IS ONE PRE-RELEASE PER SHELF OF THE MAIN REPOSITORY, tagged
# `src-<shelf>`, plus `src-attic` for files that only old history names. An
# archived file is the asset
#
#     https://github.com/kunaldawn/kdos/releases/download/src-<shelf>/<asset>
#
# where <asset> is the file's own name as GitHub stored it — `zstd-1.5.7.tar.gz`
# — or `<port>--<file>` or `<port>--<hash12>--<file>` when a shorter name is
# already taken in that release. GitHub rewrites every character outside
# [A-Za-z0-9._-] in an asset name, so the index records the name GitHub
# returned, never the one asked for. The name is only an address: the
# identity is the recipe's `sha256 =`, and a file is used only after it hashes
# to that, whatever path it came by.
#
# A FILE LARGER THAN $SRC_PART_SIZE BYTES IS STORED IN PARTS, `<asset>.part01`
# … `<asset>.partNN`, each under GitHub's 2 GiB per-asset limit. ports/fetch
# downloads every part, checks each part's own hash, joins them in order and
# checks the recipe's hash over the whole.
#
# WHICH RELEASE AND ASSET HOLD A HASH IS RECORDED IN ports/sources.idx,
# committed, format 2:
#
#     # kdos-sources-index 2                      (line 1, exactly)
#     <hash> <tag> <asset> <port>/<file> [parts=<N>:<h1>,…,<hN>]
#
# one line per archived file, sorted by hash. ports/publish writes a line only
# after GitHub reports every uploaded asset's digest equal to the bytes it
# sent. An index whose first line is not the format-2 marker is not read at
# all: fetch warns once and goes upstream for everything, so a checkout never
# takes a line of another format for an address. A hash the index does not
# name is not archived as far as fetch is concerned, and comes from upstream.
#
# APPEND-ONLY. An asset is never replaced or deleted once its digest is
# verified, with two deliberate exceptions in ports/publish: --rehome deletes
# an old copy only after the new one is verified and pushed in the index, and
# --prune=yes-delete deletes files no recipe at any v* tag names. A five-year-old
# checkout otherwise finds the exact bytes it was written against, fetched
# with this tree's index through `ports/fetch --tree`.
#
# The archive's releases are pre-releases and never "latest", so the latest
# release of the repository is always a KDOS system release. GitHub's
# immutable releases must stay OFF on the repository: the setting is
# repository-wide, and it freezes an archive release at its first
# publication, after which no source can be added to it.

KDOS_SOURCES_REPO="${KDOS_SOURCES_REPO:-kunaldawn/kdos}"
# Empty means upstream only — no archive is consulted.
KDOS_SOURCES_BASE="${KDOS_SOURCES_BASE-https://github.com/$KDOS_SOURCES_REPO/releases/download}"

# A BASE WITH NO SCHEME IS A DIRECTORY: an archive disk, or any copy of the
# release assets laid out as <dir>/<tag>/<asset>, a split file's parts beside
# each other as <asset>.partNN. It is made absolute and served as a file://
# URL, so every caller reads it through the same curl call as the network
# archive — a relative path would follow each caller's later cd.
case $KDOS_SOURCES_BASE in
    ""|*://*) ;;
    /*) KDOS_SOURCES_BASE="file://$KDOS_SOURCES_BASE" ;;
    *) KDOS_SOURCES_BASE="file://$PWD/$KDOS_SOURCES_BASE" ;;
esac

# src_base_local — true when the archive is a directory on this machine.
src_base_local() {
    [[ $KDOS_SOURCES_BASE == file://* ]]
}

SRCLIB_PORTS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRCLIB_ROOT="$(dirname "$SRCLIB_PORTS")"

# The local cache, one file per hash, gitignored, with each port directory
# holding a hard link into it. It belongs to this checkout, so
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

# At most this many assets in one archive release, parts counted one by one:
# GitHub's 1000-asset limit. A shelf that reaches it makes ports/publish stop
# on that file; there is no overflow release.
SRC_RELEASE_CAP="${KDOS_RELEASE_CAP:-1000}"

# A file larger than this is stored in parts of this size: 1900 MiB, under
# GitHub's 2 GiB per-asset limit with room for its own accounting.
# KDOS_PART_SIZE exists so a test can split a small file.
SRC_PART_SIZE="${KDOS_PART_SIZE:-1992294400}"

KDOS_SOURCES_INDEX="${KDOS_SOURCES_INDEX:-$SRCLIB_PORTS/sources.idx}"
case $KDOS_SOURCES_INDEX in
    /*) ;;
    *) KDOS_SOURCES_INDEX="$PWD/$KDOS_SOURCES_INDEX" ;;
esac

# Line 1 of a format-2 index, exactly. An index without it is not read.
SRC_INDEX_MARKER='# kdos-sources-index 2'

# src_release_tag <shelf> — the tag of the archive release holding <shelf>'s
# files; `attic` for files no current port names.
src_release_tag() {
    printf 'src-%s\n' "$1"
}

# src_part_name <asset> <i> — the asset name of part <i> of a split file.
src_part_name() {
    printf '%s.part%02d\n' "$1" "$((10#$2))"
}

# src_asset_sanitize <name> — the asset name GitHub will store for <name>:
# every character outside [A-Za-z0-9._-] becomes `.`, and leading and
# trailing dots go. It predicts; ports/publish records what GitHub returns.
src_asset_sanitize() {
    local s=${1//[^A-Za-z0-9._-]/.}
    while [[ $s == .* ]]; do s=${s#.}; done
    while [[ $s == *. ]]; do s=${s%.}; done
    printf '%s\n' "$s"
}

# src_urlencode <string> — percent-encode everything outside
# [A-Za-z0-9._~-], byte by byte, for a URL path segment.
src_urlencode() {
    local LC_ALL=C s=$1 out="" c i
    for ((i = 0; i < ${#s}; i++)); do
        c=${s:i:1}
        case $c in
            [A-Za-z0-9._~-]) out+=$c ;;
            *) printf -v c '%%%02X' "'$c"; out+=$c ;;
        esac
    done
    printf '%s\n' "$out"
}

# src_asset_url <tag> <asset> — the download URL of one asset.
src_asset_url() {
    printf '%s/%s/%s\n' "$KDOS_SOURCES_BASE" "$1" "$(src_urlencode "$2")"
}

# src_index_load [file] — read a format-2 index into
#
#   SRC_TAG[h]      the release tag
#   SRC_ASSET[h]    the stored asset name (the base name of a split file)
#   SRC_NAME[h]     <port>/<file>
#   SRC_NPARTS[h]   1, or the part count of a split file
#   SRC_PHASH[h,i]  the hash of part i, 1-based, of a split file
#
# A missing or empty file is an empty index. A file whose first line is not
# $SRC_INDEX_MARKER loads nothing and says so once on stderr. Lines that are
# blank, comments or malformed are skipped, not trusted.
declare -gA SRC_TAG=() SRC_ASSET=() SRC_NAME=() SRC_NPARTS=() SRC_PHASH=()
SRC_INDEX_WARNED=0
src_index_load() {
    local f=${1:-$KDOS_SOURCES_INDEX} first h tag asset name parts extra n i
    local -a ph
    SRC_TAG=() SRC_ASSET=() SRC_NAME=() SRC_NPARTS=() SRC_PHASH=()
    [ -s "$f" ] || return 0
    IFS= read -r first < "$f" || true
    if [ "$first" != "$SRC_INDEX_MARKER" ]; then
        if [ "$SRC_INDEX_WARNED" = 0 ]; then
            echo "⚠  ports/sources.idx is not format 2; the archive is skipped" >&2
            SRC_INDEX_WARNED=1
        fi
        return 0
    fi
    while read -r h tag asset name parts extra; do
        src_is_hash "$h" || continue
        [ -z "$extra" ] || continue
        [[ $tag =~ ^src-[a-z0-9][a-z0-9-]*$ ]] || continue
        [[ $asset =~ ^[A-Za-z0-9._-]+$ ]] || continue
        [[ $name =~ ^[^/]+/.+$ ]] || continue
        n=1
        if [ -n "$parts" ]; then
            [[ $parts =~ ^parts=([0-9]{1,2}):([0-9a-f]{64}(,[0-9a-f]{64})*)$ ]] || continue
            n=$((10#${BASH_REMATCH[1]}))
            IFS=, read -ra ph <<< "${BASH_REMATCH[2]}"
            [ "$n" -ge 2 ] && [ "${#ph[@]}" = "$n" ] || continue
            for ((i = 1; i <= n; i++)); do SRC_PHASH[$h,$i]=${ph[i-1]}; done
        fi
        SRC_TAG[$h]=$tag
        SRC_ASSET[$h]=$asset
        SRC_NAME[$h]=$name
        SRC_NPARTS[$h]=$n
    done < "$f"
}

# src_urls <hash> — every URL the archive serves the file from, one per line:
# the asset, or each part in order. Fails when the archive is off or the
# loaded index does not name the hash.
src_urls() {
    local h=$1 i n
    [ -n "$KDOS_SOURCES_BASE" ] || return 1
    [ -n "${SRC_TAG[$h]:-}" ] || return 1
    n=${SRC_NPARTS[$h]}
    if [ "$n" -lt 2 ]; then
        src_asset_url "${SRC_TAG[$h]}" "${SRC_ASSET[$h]}"
        return 0
    fi
    for ((i = 1; i <= n; i++)); do
        src_asset_url "${SRC_TAG[$h]}" "$(src_part_name "${SRC_ASSET[$h]}" "$i")"
    done
}

# src_index_problems [file] — one line per breach of the format-2 rules,
# nothing when the index holds them. ports/publish writes only indexes that
# pass, and testing/preflight.sh refuses one that does not:
#
#   - line 1 is $SRC_INDEX_MARKER; comments come before every data line
#   - a data line is <hash> <tag> <asset> <port>/<file> [parts=N:<h1>,…,<hN>],
#     N from 2 to 99 and exactly N part hashes
#   - <tag> is src-<shelf> for a shelf ports/shelves lists, or src-attic
#   - a hash is field 1 of one line only, and no part hash is any line's
#     field 1
#   - within a tag every asset name is unique ignoring case; a split file
#     holds <asset>.partNN and reserves its bare <asset> as well
#   - data lines are in `LC_ALL=C sort -k1,1` order
#
# A missing file is an empty index and passes.
src_index_problems() {
    local f=${1:-$KDOS_SOURCES_INDEX}
    [ -f "$f" ] || return 0
    # No regex intervals: not every awk has them, and one without would pass
    # every line.
    LC_ALL=C awk -v marker="$SRC_INDEX_MARKER" -v shelves="$(src_shelves | tr '\n' ' ')" '
        function ishash(x) { return length(x) == 64 && x ~ /^[0-9a-f]+$/ }
        BEGIN {
            n = split(shelves, s, " ")
            for (i = 1; i <= n; i++) if (s[i] != "") okt["src-" s[i]] = 1
            okt["src-attic"] = 1
        }
        NR == 1 {
            if ($0 != marker) print "line 1 is not \"" marker "\""
            next
        }
        /^[[:space:]]*$/ || /^#/ {
            if (data) print "line " NR ": a comment or blank line after the data"
            next
        }
        {
            data++
            if ($2 ~ /^[0-9]+$/) { print "line " NR ": a format-1 line (a release number where the tag goes)"; next }
            ok = (NF == 4 || NF == 5) && $0 !~ /^ | $|  / && ishash($1) &&
                 $2 ~ /^src-[a-z0-9][a-z0-9-]*$/ && $3 ~ /^[A-Za-z0-9._-]+$/ &&
                 $4 ~ /^[^\/]+\/./
            np = 0; m = 0
            if (ok && NF == 5) {
                ok = $5 ~ /^parts=[0-9][0-9]?:[0-9a-f,]+$/
                if (ok) {
                    split($5, pv, /[=:]/)
                    np = pv[2] + 0
                    m = split(pv[3], ph, ",")
                    for (i = 1; i <= m; i++) if (!ishash(ph[i])) ok = 0
                }
            }
            if (!ok) { print "line " NR ": malformed"; next }
            h = $1; tag = $2; asset = $3
            if (!(tag in okt)) print "line " NR ": " tag " is not src-<a shelf in ports/shelves> or src-attic"
            if (h in key) print "line " NR ": " h " is also on line " key[h]
            else key[h] = NR
            if (prev != "" && h < prev) print "line " NR ": not sorted by hash"
            prev = h
            if (NF == 5) {
                if (np < 2 || np > 99) print "line " NR ": parts=" pv[2] " — a split file has 2 to 99 parts"
                if (m != np) print "line " NR ": parts=" pv[2] " lists " m " part hash(es)"
                for (i = 1; i <= m; i++) parth[ph[i]] = NR
            }
            k = 1; nm[1] = asset
            for (i = 1; i <= np && i <= 99; i++) nm[++k] = sprintf("%s.part%02d", asset, i)
            for (i = 1; i <= k; i++) {
                a = tag SUBSEP tolower(nm[i])
                if (a in held && held[a] != NR) print "line " NR ": asset " nm[i] " in " tag " is also held by line " held[a]
                else held[a] = NR
            }
        }
        END {
            for (p in parth) if (p in key) print "line " parth[p] ": part hash " p " is also the hash of line " key[p]
        }' "$f"
}

# src_cache_path <hash> — the cache splits by leading byte only to keep each
# directory small; it has nothing to do with where the archive keeps a file.
src_cache_path() {
    printf '%s/sha256-%s/%s\n' "$KDOS_SRCCACHE" "${1:0:2}" "$1"
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
# (kdos-kpkg.h includes ksig.h); src/devtools/kdos-portup/main.c and
# testing/selftest.sh compile the same set and all three must agree.
KPKG="${KPKG_BIN:-$SRCLIB_PORTS/.kpkgbin/kpkg}"

src_kpkg_ensure() {
    local srcs=(
        "$SRCLIB_ROOT"/src/system/kdos-kpkg/*.[ch]
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
        -I"$SRCLIB_ROOT/src/system/kdos-kpkg" -o "$KPKG.tmp" \
        "$SRCLIB_ROOT"/src/system/kdos-kpkg/*.c \
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

# A REPOSITORY HOLDS PORTS DIRECTLY OR ONE SHELF DOWN: <repo>/<name>/ or
# <repo>/<shelf>/<name>/, never deeper. ports/core is shelved and every
# src/<area> is flat; both are read by the same walk, whichever tree
# ports/fetch --tree names. A directory holding a kpkgbuild is a port and is never looked inside,
# so a port's own subdirectories are not taken for ports. A port's identity is
# its bare name, whatever shelf it sits on.

# src_shelves — every shelf id ports/shelves lists, one per line. Comments and
# blank lines are skipped.
src_shelves() {
    [ -f "$SRCLIB_PORTS/shelves" ] || return 0
    awk '!/^[[:space:]]*(#|$)/ { print $1 }' "$SRCLIB_PORTS/shelves"
}

# src_port_dirs <repo> — every port directory under <repo>, one per line.
src_port_dirs() {
    local d p
    for d in "$1"/*/; do
        d=${d%/}
        if [ -f "$d/kpkgbuild" ]; then
            printf '%s\n' "$d"
            continue
        fi
        for p in "$d"/*/kpkgbuild; do
            [ -f "$p" ] && printf '%s\n' "${p%/kpkgbuild}"
        done
    done
    return 0
}

# src_port_dir <repo> <name> — the directory of port <name> under <repo>.
# Fails when there is none, and when there are two: a name on two shelves is
# ambiguous, and taking either would build whichever the walk met first.
src_port_dir() {
    local hits=() d
    case $2 in ""|*/*|.|..) return 1 ;; esac
    [ -f "$1/$2/kpkgbuild" ] && hits+=("$1/$2")
    for d in "$1"/*/"$2"; do
        [ -f "${d%/*}/kpkgbuild" ] && continue
        [ -f "$d/kpkgbuild" ] && hits+=("$d")
    done
    case ${#hits[@]} in
        0) return 1 ;;
        1) printf '%s\n' "${hits[0]}" ;;
        *) echo "port $2 is in more than one place: ${hits[*]}" >&2; return 1 ;;
    esac
}

# src_label <path> — the index label "<port>/<file>" for a path under
# ports/core, with or without its shelf: a leading component that ports/shelves lists
# is the shelf and is dropped. No shelf shares a name with a port, so a flat
# path is never mistaken for a shelved one.
declare -gA SRC_SHELF=()
src_label() {
    local rel=${1#ports/core/} s
    if [ "${#SRC_SHELF[@]}" = 0 ]; then
        while read -r s; do SRC_SHELF[$s]=1; done < <(src_shelves)
    fi
    [ -n "${SRC_SHELF[${rel%%/*}]:-}" ] && rel=${rel#*/}
    printf '%s\n' "$rel"
}

# src_port_shelf <name> [repo] — the shelf port <name> sits on under <repo>
# (default ports/core): the parent directory's name. Fails for a port that
# does not exist, and for one sitting loose in <repo> with no shelf.
src_port_shelf() {
    local repo=${2:-$SRCLIB_ROOT/ports/core} d
    d=$(src_port_dir "$repo" "$1") || return 1
    d=${d%/*}
    [ "$d" != "$repo" ] || return 1
    printf '%s\n' "${d##*/}"
}

# src_shelf_desc <shelf> — the description ports/shelves gives <shelf>,
# everything after its id. Fails for a shelf it does not list.
src_shelf_desc() {
    [ -f "$SRCLIB_PORTS/shelves" ] || return 1
    awk -v s="$1" '
        !/^[[:space:]]*(#|$)/ && $1 == s {
            if (NF > 1) sub(/^[[:space:]]*[^[:space:]]+[[:space:]]+/, "")
            else $0 = ""
            print; found = 1; exit
        }
        END { exit !found }' "$SRCLIB_PORTS/shelves"
}
