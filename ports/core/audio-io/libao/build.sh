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
# The pulse plugin calls nanosleep() without including <time.h>, which glibc
# pulls in through another header and musl does not; an undeclared function is
# an error, so the header is included for it.
export CFLAGS="$CFLAGS -include time.h"
./autogen.sh
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-alsa --enable-pulse --disable-esd --disable-arts --disable-nas
make
make DESTDIR=$PKG install
