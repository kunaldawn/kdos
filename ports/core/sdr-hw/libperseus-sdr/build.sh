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

# The receiver's Cypress FX2 firmware and its FPGA bitstreams (the .rbs
# files) are compiled into the library as data and loaded into the device
# on open: code for another processor, as upstream publishes it.
#
# configure writes -march=native, -ffast-math and -fopenmp into AM_CFLAGS
# of both makefiles. Giving AM_CFLAGS on the make command line, which every
# sub-make inherits, keeps the library portable and the phase's own flags in
# charge.
./configure --prefix=/usr --libdir=/usr/lib --disable-static
_amcflags="-Wall -DGIT_REVISION=\\\"$version\\\""
make AM_CFLAGS="$_amcflags"

# The install-data hook copies a udev rule into /etc on the build machine
# and runs groupadd and usermod there. Only the library, its header, its
# pkg-config file and the test program are installed; device access comes
# from /etc/udev/rules.d/70-kdos-sdr.rules.
make DESTDIR=$PKG install-libLTLIBRARIES install-hdrHEADERS install-pkgconfigDATA
make -C examples DESTDIR=$PKG AM_CFLAGS="$_amcflags" install-binPROGRAMS

test -f "$PKG/usr/lib/libperseus-sdr.so"
test -f "$PKG/usr/include/perseus-sdr.h"
test -x "$PKG/usr/bin/perseustest"
