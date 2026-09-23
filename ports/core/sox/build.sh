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

# THE PATCH IS A RULE-7 CASE: no flag answers a #error. formats.c reaches into
# the FILE struct to rewind a pipe and knows three libcs' layouts; musl's FILE
# is opaque, so it lands in the #else that stops the build outright. Upstream's
# own comment there gives the answer the patch takes.
patch -p1 -i "$PORT_SRC/musl-rewind-pipe.patch"

# A 2015 autoconf against GCC 15: its conftests are K&R and every one is
# rejected, which configure reports as a broken compiler.
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch"
# Every format is named, =yes where the macro takes it: configure's default
# drops a format whose library it cannot find and says nothing, where =yes
# stops the build. libltdl is the one that cannot fail loudly; LADSPA rides on
# it. The audio devices are ALSA alone — PipeWire's ALSA device is where every
# program on this image reaches the card, so libpulse would be a second path
# to the same place.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-distro=KDOS \
	--with-libltdl \
	--with-ladspa \
	--with-magic \
	--with-png \
	--with-mad \
	--with-id3tag \
	--with-lame \
	--without-twolame \
	--with-mp3=yes \
	--with-oggvorbis=yes \
	--with-opus=yes \
	--with-flac=yes \
	--with-wavpack=yes \
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
