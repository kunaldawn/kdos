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

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Ddoc=disabled \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Dnls=disabled \
	-Dximagesrc=disabled \
	-Dgtk3=disabled \
	-Dqt5=disabled \
	-Dqt6=disabled \
	-Drpicamsrc=disabled \
	-Dv4l2=enabled \
	-Dv4l2-libv4l2=enabled \
	-Dv4l2-gudev=enabled \
	-Dflac=enabled \
	-Dlame=enabled \
	-Dmpg123=enabled \
	-Dwavpack=enabled \
	-Dvpx=enabled \
	-Dpng=enabled \
	-Djpeg=enabled \
	-Dcairo=enabled \
	-Dbz2=enabled \
	-Daalib=enabled \
	-Dpulse=enabled \
	-Dsoup=enabled \
	-Dadaptivedemux2=enabled \
	-Dhls-crypto=openssl \
	-Dorc=enabled \
	-Dorc-compiler=disabled \
	-Dasm=enabled \
	-Dgdk-pixbuf=disabled \
	-Doss=disabled \
	-Doss4=disabled \
	-Djack=disabled \
	-Dlibcaca=disabled \
	-Dshout2=disabled \
	-Dspeex=disabled \
	-Dtaglib=disabled \
	-Dtwolame=disabled \
	-Ddv=disabled \
	-Ddv1394=disabled \
	-Damrnb=disabled \
	-Damrwbdec=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
