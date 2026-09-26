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

# souphttpsrc and the adaptivedemux2 HLS/DASH/MSS elements dlopen libsoup-3.0
# at run time rather than linking it, so nothing at build time notices its
# absence: without libsoup3 on the image both plugins load and register no
# element, and playbin falls back to curlhttpsrc and plugins-bad's older
# demuxers. libsoup3 is in depends for that reason, and carries
# glib-networking, without which every https URL fails at the handshake.
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
