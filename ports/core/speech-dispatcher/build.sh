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


# espeak-ng IS THE ONLY ENGINE SWITCHED ON. Every other switchable engine is
# one this image does not carry, and a module that cannot find its engine is a
# voice that appears in the list and says nothing when chosen. voxin,
# baratinoo and kali build a SHIM when their engine is missing — a module that
# dlopens a proprietary library at run time — so each is named off rather
# than left to the probe. cicero, festival, openjtalk and the generic runner
# have no switch and are always built; each drives an external program.
#
# THE AUDIO SIDE IS ALSA AND libao IS OFF. Everything on this image reaches
# the card through PipeWire's ALSA device, so a second output path would be a
# second answer to which device speech comes out of.
#
# The audio back end is loaded with plain dlopen, not libltdl.
#
# The Python side stays off: spd-conf imports pyxdg at start-up, and with no
# port providing it the tool would ship and fail on its first line.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-ltdl \
	--with-espeak-ng \
	--without-espeak \
	--without-flite \
	--without-pico \
	--without-ivona \
	--without-ibmtts \
	--without-voxin \
	--without-baratinoo \
	--without-kali \
	--with-alsa \
	--without-libao \
	--without-pulse \
	--without-pipewire \
	--without-nas \
	--without-oss \
	--without-systemdsystemunitdir \
	--without-systemduserunitdir \
	--disable-python
make
make DESTDIR=$PKG install
