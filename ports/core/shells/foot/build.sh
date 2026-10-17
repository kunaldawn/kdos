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

export CFLAGS="${CFLAGS/-O2/-O3} -fno-strict-aliasing"
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dgrapheme-clustering=enabled \
	-Dterminfo=enabled \
	-Dterminfo-base-name=foot-extra \
	-Dime=true \
	-Ddocs=enabled \
	-Dutmp-backend=none \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
for f in foot-server footclient; do
	desktop-file-edit --set-key=NoDisplay --set-value=true "$PKG/usr/share/applications/$f.desktop"
done
desktop-file-edit --set-name=Terminal "$PKG/usr/share/applications/foot.desktop"
