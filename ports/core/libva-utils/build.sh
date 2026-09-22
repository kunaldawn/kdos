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

# DRM and Wayland only, matching the backends libva itself is built with.
#
# Every backend option here is a `combo` whose default is `auto`, and `auto`
# means "build it if the dependency happens to be found". An auto backend is
# therefore decided by whatever is installed in the chroot at the moment, so
# every one is pinned: `true` makes a missing dependency fail setup instead of
# dropping a tool, and `false` is the only spelling that keeps x11 out even
# when libX11 is present for Xwayland.
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Ddrm=true \
	-Dwayland=true \
	-Dx11=false \
	-Dwin32=false \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
