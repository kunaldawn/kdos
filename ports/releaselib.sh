# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   ports/releaselib.sh — the release index, written
# ---------------------------------
#
# Sourced by script/phases/70_image/010_binhost.sh, ports/publish and
# testing/selftest.sh. Nothing here downloads, uploads or signs; it writes and
# splits the index `kdos update` reads (src/system/kdos-tools/relidx.c, which
# is the reader and documents the format), so the producers cannot disagree
# about it.
#
#     # kdos-update-index 1
#     version <release>
#     releases <URL release names are joined to>      (published only)
#     <sha256> <size> <release> <asset> <path>
#
# <path> is what a client asks for: `binhost/<file>` for a binhost's files,
# `binhost/swcatalog/xml/kdos-binhost.xml` for its catalogue,
# `image/<file>` for a slot image's. <asset> is FLAT — the path below its
# lane with every `/` written `--` — because a GitHub release holds no
# directories. <release> is `-` for a file beside the index, which is how a
# producer's own directory is laid out, or the GitHub release holding it.
#
# A PUBLISHED BINHOST IS SHARDED. GitHub keeps at most 1,000 assets in one
# release ($SRC_RELEASE_CAP from ports/srclib.sh, or $KDOS_RELEASE_CAP, or
# 1000 here), and a binhost is a package and its sidecar for every port, so
# its files go, sorted by path, into `binhost-<version>-01`, `-02`, … of at
# most that many each. A slot image is a manifest, a few parts and a delta or
# two, and goes into `image-<version>` whole.

REL_INDEX_MARKER='# kdos-update-index 1'
REL_INDEX_FILE=kdos-update.idx
REL_CAP="${SRC_RELEASE_CAP:-${KDOS_RELEASE_CAP:-1000}}"

# rel_asset <path below the lane> — the flat asset name a producer writes
# the file under.
rel_asset() {
    local p=$1
    printf '%s\n' "${p//\//--}"
}

# rel_gh_name <asset> — the name GitHub will store it as: every character
# outside [A-Za-z0-9._-] becomes `.`. ports/publish records the name GitHub
# returned and only predicts with this.
rel_gh_name() {
    local a=$1
    printf '%s\n' "${a//[!A-Za-z0-9._-]/.}"
}

# rel_entry <dir> <asset> <path> — one index line for <dir>/<asset> beside
# the index: "<sha256> <size> - <asset> <path>".
rel_entry() {
    local f="$1/$2" sha size
    [ -f "$f" ] || { echo "releaselib: no file $f" >&2; return 1; }
    sha=$(sha256sum "$f" | cut -d' ' -f1) || return 1
    size=$(stat -c %s "$f") || return 1
    printf '%s %s - %s %s\n' "$sha" "$size" "$2" "$3"
}

# rel_write <out> <version> [<releases URL>] — the index from entry lines on
# stdin, sorted by path in C order (the reader refuses any other order) and
# written whole: to <out>.tmp, then renamed.
rel_write() {
    local out=$1 version=$2 releases=${3:-}
    {
        echo "$REL_INDEX_MARKER"
        echo "version $version"
        [ -z "$releases" ] || echo "releases $releases"
        LC_ALL=C sort -k5,5 -u
    } > "$out.tmp" || return 1
    mv -f "$out.tmp" "$out"
}

# rel_entries <index> — its entry lines, header and comments dropped.
rel_entries() {
    awk 'NR == 1 || /^#/ || /^version / || /^releases / || NF == 0 { next } { print }' "$1"
}

# rel_shard <prefix> [<cap>] — entry lines on stdin, sorted by path, each
# given the release it goes to: the <k>th line (from 0) to
# <prefix>-<k / cap + 1>, two digits. Lines whose release is not `-` are
# passed through unchanged.
rel_shard() {
    local prefix=$1 cap=${2:-$REL_CAP}
    [[ $cap =~ ^[1-9][0-9]*$ ]] || { echo "releaselib: shard size '$cap' is not a positive number" >&2; return 1; }
    LC_ALL=C sort -k5,5 | awk -v p="$prefix" -v cap="$cap" '
        $3 != "-" { print; next }
        { $3 = sprintf("%s-%02d", p, int(k / cap) + 1); k++; print }'
}

# rel_release_counts — "<release> <assets>" for entry lines on stdin.
rel_release_counts() {
    awk '{ n[$3]++ } END { for (r in n) print r, n[r] }' | LC_ALL=C sort
}
