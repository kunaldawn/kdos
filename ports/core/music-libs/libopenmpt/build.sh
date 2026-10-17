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

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-openmpt123 --disable-examples --disable-tests \
	--disable-doxygen-doc \
	--with-zlib --with-mpg123 --with-ogg --with-vorbis --with-vorbisfile \
	--with-pulseaudio --without-portaudio --without-portaudiocpp \
	--without-sdl2 --with-sndfile --with-flac
make
make DESTDIR=$PKG install
