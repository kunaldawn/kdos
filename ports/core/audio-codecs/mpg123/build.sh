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

# ALSA is the one output module: it is the only audio library in --with-audio
# that is a port here, and --with-default-audio pins the runtime search to it so
# no module is probed that the package does not carry. HTTP streams go through
# the internal code for plain HTTP and an exec of curl or wget for HTTPS.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-components \
	--enable-modules \
	--with-audio=alsa \
	--with-default-audio=alsa \
	--with-network=exec \
	--enable-ipv6
make
make DESTDIR=$PKG install
