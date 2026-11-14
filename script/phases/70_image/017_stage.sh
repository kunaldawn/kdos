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
# Trim the image stage to the base: every installed package outside ports/base
# and its closure is removed from it, and the ISO carries what is left.
#
# THIS RUNS IN THE STAGE, NEVER IN build/fs. script/chroot/exec.sh roots every
# 70_image step from this one on in an overlay over build/fs, this step with
# an empty upper layer, so a removal here writes whiteouts into
# build/image-upper and build/fs keeps every package for the next build and
# the binhost. The step refuses to run unless / is that overlay: the same
# `kpkg remove` over build/fs would delete most of the build.
#
# THE SET IS script/lib/imageset.py's (ports/base, ports/buildonly), read from
# the repository at /kdos. kpkg remove takes the dropped names in one run, so
# no removal is refused for a reverse dependency that is also going, and its
# index triggers rebuild the caches (fonts, icons, MIME, desktop entries,
# ld.so.cache) once for the trimmed tree. Every later step — the cleanup, the
# orphan sweep, the launchers, the indexes and 087_gates.sh's linkcheck and
# entrycheck — reads this tree, so a package the base needs at run time and
# that the closure missed fails the image there.
#
# THE IMAGE CARRIES THE RUN-TIME FACET OF EACH PACKAGE. Once the set is the
# base, `kpkg facets --strip` removes every package's development and
# documentation files from the stage (libkpkg's kp_facet_of: headers, static
# libraries, CMake, pkg-config and aclocal files, GIR and VAPI files, the -l
# links, man sections 0, 2 and 3, share/doc except the handbooks and the two
# books, info, gtk-doc; and what a recipe's facet_dev = adds, gcc's compiler
# among it) and lists each in /var/lib/kpkg/db/.facets/<name>,
# and the stage's /etc/kpkg.conf is rewritten to say FACETS=run, so the
# medium and every machine installed from it leave those files out of each
# later install too. build/fs keeps FACETS=all and every file. 087_gates.sh's
# linkcheck runs after, so a program that links through a removed -l link
# fails the image.
#
# Under KDOS_IMAGE=full this step does nothing and the image is build/fs.

set -e
source script/phases/70_image/phase.env

if [ "${KDOS_IMAGE:-base}" = full ]; then
    echo "[stage] KDOS_IMAGE=full — the image carries every installed package"
    exit 0
fi

rootfs=$(awk '$2 == "/" { t = $3 } END { print t }' /proc/mounts)
if [ "$rootfs" != overlay ]; then
    echo "FATAL: / is $rootfs, not the image stage's overlay; refusing to remove packages from build/fs" >&2
    exit 1
fi

python3 /kdos/script/lib/imageset.py --tree /kdos --check
drop=$(python3 /kdos/script/lib/imageset.py --tree /kdos --root / --drop)
n=$(printf '%s\n' "$drop" | grep -c . || true)
echo "[stage] removing $n package(s) outside the base"
if [ "$n" -gt 0 ]; then
    printf '%s\n' "$drop" | xargs kpkg remove >/dev/null
fi
left=$(printf '%s\n' "$drop" | while IFS= read -r p; do
    [ -n "$p" ] && [ -f "/var/lib/kpkg/db/$p" ] && printf '%s ' "$p"
done)
if [ -n "$left" ]; then
    echo "FATAL: still installed after the removal: $left" >&2
    exit 1
fi
python3 /kdos/script/lib/imageset.py --tree /kdos --root / --sizes

echo "[stage] removing the development and documentation facets"
strip=$(kpkg facets --strip)
printf '%s\n' "$strip" | sed 's/^==> /[stage] /'
conf=$(awk '!/^FACETS=/' /etc/kpkg.conf)
printf '%s\nFACETS="${KPKG_FACETS:-run}"\n' "$conf" > /etc/kpkg.conf.new
mv /etc/kpkg.conf.new /etc/kpkg.conf
grep -qx 'FACETS="${KPKG_FACETS:-run}"' /etc/kpkg.conf

touch /kdos/build/image-stage.ok
echo "[stage] the image is the base: $(python3 /kdos/script/lib/imageset.py --tree /kdos --root / --keep | wc -l) packages"
