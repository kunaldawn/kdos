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
# Index the manual tree so `apropos` and `whatis` have something to search.
#
# `man foo` works without an index because it falls back to walking the
# filesystem; apropos and whatis do not — they reach mansearch() only. Without
# a mandoc.db they exit 0 printing nothing on a fully populated manual tree,
# which is indistinguishable from "nothing matches" and is what the user
# believes.
#
# THE DATABASE IS PER MANUAL ROOT: mandoc's indexer writes `mandoc.db` into
# each directory it is given, and mansearch() opens the one in each element of
# the manpath. The roots below are the two the port compiles into
# MANPATH_DEFAULT; only the ones that exist are passed, because the indexer
# reports a missing argument directory as an error.
#
# RUNS AFTER 00_orphans.sh, so the pages of a swept package are not indexed
# into an answer that names a program the image does not carry, and before
# 01_initramfs.sh and 02_iso.sh, which carry the tree into the image.
# Lexicographic order does the sequencing.

set -e
source script/packaging.env.sh

# `makewhatis` is the name mandoc's BINM_MAKEWHATIS installs into /usr/sbin;
# the binary behind it is mandoc itself.
ROOTS="/usr/share/man /usr/local/share/man"

if ! command -v makewhatis >/dev/null 2>&1; then
    echo "[whatis] makewhatis is not installed — skipping" >&2
    exit 0
fi

dirs=""
for root in $ROOTS; do
    [ -d "$root" ] || continue
    # THE STALE INDEX GOES FIRST. The build tree is incremental, and a root
    # that no longer holds a page keeps its old database otherwise — the one
    # case where apropos answers with programs the image does not carry.
    rm -f "$root/mandoc.db"
    dirs="$dirs $root"
done

if [ -z "$dirs" ]; then
    echo "FATAL: none of $ROOTS exists; nothing installed a manual page." >&2
    exit 1
fi

echo "[whatis] indexing$dirs"

# NOT `set -e`-FATAL. The indexer sets a non-zero status for a single
# unreadable page or dangling link inside the tree and still writes a complete
# database for everything else, so the database is the assertion and the status
# is a warning.
makewhatis $dirs || echo "[whatis] makewhatis reported errors — see above" >&2

indexed=0
for root in $dirs; do
    [ -s "$root/mandoc.db" ] || continue
    echo "[whatis] $root/mandoc.db: $(wc -c < "$root/mandoc.db") bytes"
    indexed=$(( indexed + 1 ))
done

# A root with no manual pages in it gets no database by design, so one empty
# root is not an error — all of them being empty is.
if [ "$indexed" -eq 0 ]; then
    echo "FATAL: no mandoc.db was written under$dirs." >&2
    echo "       apropos and whatis would answer nothing for every query." >&2
    exit 1
fi
