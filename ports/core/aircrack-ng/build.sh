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

autoreconf -f -i

# `iw` IS A HARD PREREQUISITE AND IS IN depends FOR THAT REASON. airmon-ng puts
# an interface into monitor mode by calling it; without iw the whole suite
# installs and every capture fails on an interface it cannot reconfigure, which
# reads as a card that does not support monitor mode.
#
# THE HONEST USE HERE IS DIAGNOSIS OF YOUR OWN LINK. With hostapd shipped, this
# machine can BE the access point for an island network, and the questions that
# then matter — which channel is congested, why does this client keep
# deauthenticating, is the retry rate the reason throughput collapsed — are
# physical-layer questions no other tool on this system can answer.
#
# --with-experimental brings in the tools that need libnl; --disable-asan is
# not passed because it is already off, and sqlite is what airolib-ng stores a
# precomputed table in.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-static \
	--with-experimental \
	--without-opt
make
make DESTDIR=$PKG install
