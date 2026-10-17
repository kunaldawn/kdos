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

patch -p1 -i "$PORT_SRC/musl-rewind-pipe.patch"

export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch"
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-distro=KDOS \
	--with-libltdl \
	--with-ladspa \
	--with-magic \
	--with-png \
	--with-mad \
	--with-id3tag \
	--with-lame \
	--with-twolame=yes \
	--with-mp3=yes \
	--with-oggvorbis=yes \
	--with-opus=yes \
	--with-flac=yes \
	--with-wavpack=yes \
	--with-gsm=yes \
	--with-sndfile=yes \
	--without-amrwb \
	--without-amrnb \
	--with-alsa=yes \
	--without-ao \
	--without-pulseaudio \
	--without-sndio \
	--without-oss \
	--without-sunaudio \
	--without-coreaudio \
	--without-waveaudio
make
make DESTDIR=$PKG install
