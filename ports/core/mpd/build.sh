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
# httpd and snapcast outputs would make a daemon that opens a listening socket
# nobody asked it to open. nlohmann_json is linked only by snapcast and qobuz, so
# with both off it would build nothing and is disabled with them. zeroconf would advertise the daemon on the LAN, and udisks
# (the only user of dbus here) has no udisks2 daemon to answer it. What remains
# beside the streams is a local library indexed into sqlite; the output plugins
# are alsa and pipewire for the tty1/session split.
#
# EVERY feature option is named, enabled with its port in depends or disabled,
# because meson's `auto` turns on whatever the chroot happens to hold and a
# dependency() that finds nothing tries subprojects/*.wrap — a download.
# --wrap-mode=nofallback makes that a configure error instead.
#
# ffmpeg is the catch-all decoder. mpg123 is named ahead of it for MP3 because
# it is gapless and reads the LAME header; libid3tag supplies MP3 tags and
# ReplayGain; mikmod plays tracker modules, which this ffmpeg has no libopenmpt
# for; fluidsynth plays MIDI through a SoundFont the user supplies with
# `soundfont` in the decoder block, since none is shipped. mad, faad and
# wavpack are off because ffmpeg decodes the same files. libsamplerate is the
# resampler mpd uses when mpd.conf names none; soxr is the one a `resampler`
# block with `plugin "soxr"` selects.
# cdio_paranoia is the cdda:// input, an audio CD played from the drive through
# libcdio-paranoia's error-correcting reads; iso9660 opens an .iso as a
# directory of music files.
# lame and vorbisenc are the recorder output's encoders. ICU collates and folds
# case in the library, which also leaves iconv unused.
#
# The manual pages, mpd(1) and mpd.conf(5), are Sphinx output; the HTML manual
# is off.
meson setup build \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=lib \
	--buildtype=release \
	--wrap-mode=nofallback \
	-Dalsa=enabled \
	-Dpipewire=enabled \
	-Dpulse=disabled \
	-Djack=disabled \
	-Dsndio=disabled \
	-Dao=disabled \
	-Dopenal=disabled \
	-Doss=disabled \
	-Dshout=disabled \
	-Dsolaris_output=disabled \
	-Dhttpd=false \
	-Dsnapcast=false \
	-Dnlohmann_json=disabled \
	-Dffmpeg=enabled \
	-Dflac=enabled \
	-Dvorbis=enabled \
	-Dopus=enabled \
	-Dsndfile=enabled \
	-Dmpg123=enabled \
	-Did3tag=enabled \
	-Dmikmod=enabled \
	-Dfluidsynth=enabled \
	-Dmad=disabled \
	-Dfaad=disabled \
	-Dwavpack=disabled \
	-Dadplug=disabled \
	-Daudiofile=disabled \
	-Dgme=disabled \
	-Dmodplug=disabled \
	-Dopenmpt=disabled \
	-Dmpcdec=disabled \
	-Dsidplay=disabled \
	-Dtremor=disabled \
	-Dwildmidi=disabled \
	-Dchromaprint=disabled \
	-Dlame=enabled \
	-Dvorbisenc=enabled \
	-Dtwolame=disabled \
	-Dshine=disabled \
	-Dsoxr=enabled \
	-Dlibsamplerate=enabled \
	-Dsqlite=enabled \
	-Dicu=enabled \
	-Diconv=disabled \
	-Dpcre=enabled \
	-Dexpat=enabled \
	-Dzlib=enabled \
	-Dbzip2=disabled \
	-Dzzip=disabled \
	-Diso9660=enabled \
	-Dcdio_paranoia=enabled \
	-Dcurl=enabled \
	-Dmms=disabled \
	-Dnfs=disabled \
	-Dsmbclient=disabled \
	-Dwebdav=disabled \
	-Dqobuz=disabled \
	-Dupnp=disabled \
	-Dlibmpdclient=disabled \
	-Dudisks=disabled \
	-Ddbus=disabled \
	-Dzeroconf=disabled \
	-Dio_uring=enabled \
	-Dsyslog=enabled \
	-Dipv6=enabled \
	-Dsystemd=disabled \
	-Ddocumentation=enabled \
	-Dhtml_manual=false \
	-Dmanpages=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
