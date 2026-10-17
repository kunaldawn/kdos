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
# Write the fontconfig cache over the finished tree, into
# /usr/lib/fontconfig/cache, and assert that fontconfig accepts it.
#
# Without a valid system cache every user's first fontconfig client scans every
# face on the system, and on the live medium that is every boot.
#
# THE FONT DIRECTORIES' MTIMES ARE CLAMPED FIRST. Under SOURCE_DATE_EPOCH
# fc-cache stores the epoch as each directory's mtime in its cache, and a
# directory whose real mtime differs makes that cache stale: fc-cache deletes it
# as an invalid cache file and a client rescans. The directories in the image
# carry the epoch too (mksquashfs stamps every inode with it), so the clamp is
# what the medium shows anyway.
#
# RUNS AFTER 040_orphans.sh, so a swept package's faces are not cached, and
# before 090_initramfs.sh and 110_iso.sh, which carry the tree into the image.

set -e
source script/phases/70_image/phase.env

FONTS=/usr/share/fonts
CACHE=/usr/lib/fontconfig/cache

if ! command -v fc-cache >/dev/null 2>&1; then
    echo "[fonts] fc-cache is not installed — skipping" >&2
    exit 0
fi
if [ ! -d "$FONTS" ]; then
    echo "FATAL: $FONTS does not exist; nothing installed a font." >&2
    exit 1
fi
: "${SOURCE_DATE_EPOCH:?SOURCE_DATE_EPOCH is not set}"

find "$FONTS" -type d -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
rm -f "$CACHE"/*.cache-*

fc-cache -sf

n=$(find "$CACHE" -name '*.cache-*' | wc -l)
if [ "$n" -eq 0 ]; then
    echo "FATAL: fc-cache wrote no cache file under $CACHE." >&2
    exit 1
fi

# THE SECOND PASS IS THE ASSERTION: it checks each directory as a client does
# at start. A valid cache is skipped; a stale one is written again
# ("caching") and the old file reported as invalid.
if fc-cache -sv 2>&1 | grep -Eq 'invalid cache file|caching, new cache contents'; then
    echo "FATAL: fontconfig rejects the cache it just wrote under $CACHE." >&2
    exit 1
fi
echo "[fonts] $n cache files, $(fc-list | wc -l) faces"
