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

# NO sensors-detect ON THE INSTALL LIST. That script probes the SMBus by
# writing to it, which upstream itself warns can hang a machine or corrupt
# EEPROMs; every sensor a modern kernel can read is already bound by a hwmon
# driver at boot, so what is wanted here is the READER. The makefile installs
# it unconditionally, so it is removed from the package after the install.
# PROG_EXTRA is empty: its one program, sensord, needs rrdtool.
#
# There is no configure: everything is a make variable, and every one must be
# passed to both stages. ETCDIR differing between them leaves the library
# looking for sensors.conf under a prefix that is not where it was installed.
# BUILD_STATIC_LIB=0: the makefile builds and installs libsensors.a by default.
make PREFIX=/usr LIBDIR=/usr/lib ETCDIR=/etc MANDIR=/usr/share/man PROG_EXTRA= BUILD_STATIC_LIB=0
make PREFIX=/usr LIBDIR=/usr/lib ETCDIR=/etc MANDIR=/usr/share/man PROG_EXTRA= BUILD_STATIC_LIB=0 DESTDIR=$PKG install
rm -f "$PKG/usr/sbin/sensors-detect" \
	"$PKG/usr/share/man/man8/sensors-detect.8" \
	"$PKG/usr/share/zsh/site-functions/_sensors-detect"
