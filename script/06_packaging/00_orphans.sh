#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝  ╚═════╝  ╚═════╝ ╚══════╝
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
source script/packaging.env.sh

DB=/var/lib/kpkg/db
# The same list kpkg itself resolves against, so "has a port" means exactly what
# it means to a build.
REPOS="/ports/core /kdos/src/packages /kdos/src/desktop /kdos/src/libs"

[ -d "$DB" ] || { echo "[KDOS] no package database — nothing to sweep"; exit 0; }

orphans=""
for pkg in $(ls "$DB"); do
	found=0
	for repo in $REPOS; do
		[ -f "$repo/$pkg/kpkgbuild" ] && { found=1; break; }
	done
	[ "$found" = 1 ] || orphans="$orphans $pkg"
done

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
