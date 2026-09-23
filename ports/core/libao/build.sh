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

# ALSA and pulse both reach pipewire here, pulse through pipewire-pulse. esd,
# arts and nas have no server here, and a plugin for one fails at open() rather
# than at configure. The sndio and roar plugins are chosen by a header probe
# with no switch; neither is a port. --enable-pulse does not fail configure
# when libpulse is missing, so the dependency is what guarantees the plugin.
./autogen.sh
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-alsa --enable-pulse --disable-esd --disable-arts --disable-nas
make
make DESTDIR=$PKG install
