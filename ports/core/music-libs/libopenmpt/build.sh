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

# Module samples compressed as MP3 or Vorbis decode through mpg123 and
# libvorbis; without them those modules load with silent instruments.
# openmpt123 plays through libpulse, which PipeWire answers, and writes files
# through libsndfile and FLAC. PortAudio and SDL2 would be second outputs for
# the same player and are left out. The doxygen reference is not built.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-openmpt123 --disable-examples --disable-tests \
	--disable-doxygen-doc \
	--with-zlib --with-mpg123 --with-ogg --with-vorbis --with-vorbisfile \
	--with-pulseaudio --without-portaudio --without-portaudiocpp \
	--without-sdl2 --with-sndfile --with-flac
make
make DESTDIR=$PKG install
