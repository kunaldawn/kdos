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

# THE PLAYER AND THE PLAYBACK ARE SEPARATE PROCESSES, which is the property
# worth having here: the music keeps going when the terminal it was started
# from is closed, when the session is locked, and when the client is killed —
# and any client can attach, including one over ssh from another machine on
# the island network. That is a different thing from mpv, which plays a file
# in front of you.
#
# Every GUI and network-fetch feature is off: no curl input plugin fetching
# streams and no upnp. What remains is a local library indexed into
# sqlite. The output plugins are alsa and pipewire for the tty1/session split.
meson setup build \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=lib \
	--buildtype=release \
	-Dalsa=enabled \
	-Dpipewire=enabled \
	-Dpulse=disabled \
	-Djack=disabled \
	-Dsndio=disabled \
	-Dffmpeg=enabled \
	-Dflac=enabled \
	-Dvorbis=enabled \
	-Dopus=enabled \
	-Dsqlite=enabled \
	-Dcurl=disabled \
	-Dupnp=disabled \
	-Dsystemd=disabled \
	-Ddocumentation=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
