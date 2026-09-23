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


# espeak-ng IS THE ONLY OUTPUT MODULE ENABLED. The others are engines this
# image does not carry, and a module that cannot find its engine is a voice
# that appears in the list and says nothing when chosen.
#
# THE AUDIO SIDE IS ALSA AND libao IS OFF. Everything on this image reaches
# the card through PipeWire's ALSA device, so a second output path would be a
# second answer to which device speech comes out of.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--with-espeak-ng \
	--without-espeak \
	--without-flite \
	--without-pico \
	--with-alsa \
	--without-libao \
	--without-pulse \
	--without-pipewire \
	--without-nas \
	--disable-python
make
make DESTDIR=$PKG install
