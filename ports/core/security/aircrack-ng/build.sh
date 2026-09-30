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
# reads as a card that does not support monitor mode. pciutils and usbutils
# are in depends for the same reason: airmon-ng names a card's chipset and
# driver with `lspci -d` and `lsusb -d`, and exits when either is missing.
#
# THE HONEST USE HERE IS DIAGNOSIS OF YOUR OWN LINK. With hostapd shipped, this
# machine can BE the access point for an island network, and the questions that
# then matter — which channel is congested, why does this client keep
# deauthenticating, is the retry rate the reason throughput collapsed — are
# physical-layer questions no other tool on this system can answer.
#
# --with-experimental brings in the tools that need libnl; --disable-asan is
# not passed because it is already off, and sqlite is what airolib-ng stores a
# precomputed table in. OpenSSL is the crypto backend; gcrypt is the other and
# is off. hwloc is the cracker's CPU topology and thread affinity, and
# --enable-hwloc only permits the probe, which turns it off without an error
# when the library is missing, so hwloc is in depends. libpcre (the ESSID regex
# filter) has no switch and is not a port, so it is never found.
export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-static \
	--with-experimental \
	--enable-libnl \
	--with-sqlite3 \
	--without-gcrypt \
	--enable-hwloc
make
make DESTDIR=$PKG install
