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

# FLAC input to oggenc and ogg123 (libao, with curl for streams and opusfile for
# Opus) are declared; a missing library only warns and drops the feature, so
# both are stated. libspeex and libkate are probed for and are not ports.
./configure --prefix=/usr \
	--disable-nls \
	--with-flac \
	--enable-ogg123 \
	--without-speex \
	--without-kate
make
make DESTDIR=$PKG install
