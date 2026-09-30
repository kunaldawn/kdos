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
# Remove packages whose PORT no longer exists.
#
# The build tree is incremental and `cp` never deletes, so a port removed from
# `ports/` keeps its package installed forever unless something sweeps it.
# `fs/` is manifest-guarded and needs no such pass; PACKAGES are not. Skip this
# and a whole deleted desktop still rides into the ISO — hundreds of megabytes
# of binaries no recipe describes, and a portal backend with no port still
# advertising itself to xdg-desktop-portal.
#
# Runs inside the chroot, before the ISO is rolled.

set -e
source script/phases/70_image/phase.env

DB=/var/lib/kpkg/db
# Every repository a phase's PORT_REPO names, and src/libs, searched the way
# kpkg searches them: a port sits directly under a repository or one shelf
# down. A repository or a shelf level missing here makes every package in it
# an orphan, and the sweep below deletes them all from the image.
REPOS="/ports/core /kdos/src/system /kdos/src/art /kdos/src/desktop /kdos/src/daemons /kdos/src/libs"

[ -d "$DB" ] || { echo "[KDOS] no package database — nothing to sweep"; exit 0; }

has_port() {
	local repo d
	for repo in $REPOS; do
		for d in "$repo/$1" "$repo"/*/"$1"; do
			[ -f "$d/kpkgbuild" ] && return 0
		done
	done
	return 1
}

# NO PORT FOUND ANYWHERE IS A BROKEN MOUNT OR A LAYOUT THE LOOKUP MISREADS, not
# an empty distribution: every installed package would read as an orphan and
# the ISO would ship with nothing in it. Stop the build instead.
nports=$(find /ports/core -mindepth 2 -maxdepth 3 -name kpkgbuild 2>/dev/null | wc -l)
if [ "$nports" -eq 0 ]; then
	echo "[KDOS] ERROR: no kpkgbuild under /ports/core — refusing to sweep" >&2
	exit 1
fi

orphans=""
norphans=0
ninstalled=0
for pkg in $(ls "$DB"); do
	ninstalled=$((ninstalled + 1))
	has_port "$pkg" && continue
	orphans="$orphans $pkg"
	norphans=$((norphans + 1))
done

# The same failure seen from the other side: a lookup that misses a whole
# repository or shelf level still finds some ports. Retiring half the system in
# one pass is never a port removal.
if [ $((norphans * 2)) -gt "$ninstalled" ]; then
	echo "[KDOS] ERROR: $norphans of $ninstalled installed packages have no port — refusing to sweep" >&2
	exit 1
fi

if [ -z "$orphans" ]; then
	echo "[KDOS] no orphaned packages — every installed package has a port"
	exit 0
fi

echo "[KDOS] removing packages with no port:"
for pkg in $orphans; do
	echo "  $pkg"
done

# One at a time and never `set -e`-fatal: an orphan whose manifest is damaged
# must not stop the build from shipping. kpkgdel reports what it could not do.
for pkg in $orphans; do
	kpkgdel "$pkg" || echo "  [WARN] $pkg: kpkgdel failed, left in place"
done

echo "[KDOS] orphan sweep done"
