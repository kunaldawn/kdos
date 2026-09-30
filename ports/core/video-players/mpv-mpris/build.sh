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

# A C plugin mpv loads from /etc/mpv/scripts for every user; mpv is built with
# -Dcplugins=enabled for it. It registers org.mpris.MediaPlayer2.mpv, which is
# what kdos-mpctl sends the media keys to and what the panel's now-playing
# cell reads. The install rule writes the plugin under /usr/lib/mpv-mpris and
# links it into /etc/mpv/scripts.
export CFLAGS="$CFLAGS -Wno-error"
make
make DESTDIR=$PKG PREFIX=/usr install-system
