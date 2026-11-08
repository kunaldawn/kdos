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
# Write the AppStream catalogue of the installed ports,
# /usr/share/swcatalog/xml/kdos-ports.xml, over the finished tree.
#
# Discover's KDOS backend lists host software from this file and kpkg's
# database. The build skips the helper that rewrites it after every install,
# so without this step the image carries no catalogue and Discover lists no
# installed port until the first install from Discover writes one.
#
# THE WHOLE FILE IS WRITTEN FROM KPKG'S MANIFESTS: every application a package
# installs, by its own metainfo or by its desktop entry. A tree with the
# catalogue writer and no component in its output means the manifests or the
# metainfo were not read, which no image should ship.
#
# RUNS AFTER 040_orphans.sh, so a swept package is not offered as installed,
# and before 087_gates.sh, 090_initramfs.sh and 110_iso.sh. The step number does
# the sequencing.

set -e
source script/phases/70_image/phase.env

OUT=/usr/share/swcatalog/xml/kdos-ports.xml

if ! command -v kdos-discover-catalog >/dev/null 2>&1; then
    echo "[swcatalog] kdos-discover-catalog is not installed — skipping" >&2
    exit 0
fi

kdos-discover-catalog --root / --origin kdos-ports --out "$OUT"

n=$(grep -c '<component[ >]' "$OUT" || true)
if [ "$n" -eq 0 ]; then
    echo "FATAL: $OUT lists no application." >&2
    exit 1
fi
echo "[swcatalog] $n components in $OUT"
