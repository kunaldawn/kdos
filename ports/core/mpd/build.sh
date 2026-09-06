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
# curl is the HTTP input plugin and the Icy metadata parser with it, which is
# what makes a stream URL playable at all and what gives one a title to show.
# Nothing else that reaches the network is on: upnp, webdav and qobuz would each
# put a library browser or a service account behind a music player, and the
# httpd output plugin would make a daemon that opens a listening socket nobody
# asked it to open. What remains beside the streams is a local library indexed
# into sqlite; the output plugins are alsa and pipewire for the tty1/session
# split.
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
	-Dcurl=enabled \
	-Dwebdav=disabled \
	-Dqobuz=disabled \
	-Dhttpd=false \
	-Dupnp=disabled \
	-Dsystemd=disabled \
	-Ddocumentation=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
