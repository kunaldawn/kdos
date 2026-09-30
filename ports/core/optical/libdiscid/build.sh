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

# Reads an audio CD's table of contents into a MusicBrainz disc ID: Picard's
# "Lookup CD" goes through python-discid to this library, and with no drive
# nothing here is ever called. --enable-use-https makes the submission URL it
# prints an https one.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-use-https
make
make DESTDIR=$PKG install
