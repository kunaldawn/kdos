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
# Opus) rest on depends, not on the flags: --with-flac and --enable-ogg123 are
# configure's defaults, and a missing library still only warns and drops the
# feature. libspeex and libkate are probed for and are not ports.
./configure --prefix=/usr \
	--disable-nls \
	--with-flac \
	--enable-ogg123 \
	--without-speex \
	--without-kate
make
make DESTDIR=$PKG install
