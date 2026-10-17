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

export CFLAGS="$CFLAGS -D_LARGEFILE64_SOURCE"
export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dudevdir=/lib/udev \
	-Dbpf=enabled \
	-Dgconv=disabled \
	-Djpeg=enabled \
	-Dlibdvbv5=enabled \
	-Dqv4l2=disabled \
	-Dqvidcap=disabled \
	-Dv4l2-tracer=enabled \
	-Dv4l-plugins=true \
	-Dv4l-utils=true \
	-Dv4l-wrappers=true \
	-Dv4l2-compliance-libv4l=true \
	-Dv4l2-ctl-libv4l=true \
	-Dv4l2-ctl-stream-to=true \
	-Ddoxygen-doc=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
