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
# There is no configure. The makefile takes PREFIX/MANDIR and DESTDIR only,
# and ETCDIR must be passed to both stages or the library looks for
# sensors.conf under a prefix that is not where it was installed.
make PREFIX=/usr LIBDIR=/usr/lib ETCDIR=/etc MANDIR=/usr/share/man PROG_EXTRA=
make PREFIX=/usr LIBDIR=/usr/lib ETCDIR=/etc MANDIR=/usr/share/man PROG_EXTRA= DESTDIR=$PKG install
rm -f "$PKG/usr/sbin/sensors-detect" \
	"$PKG/usr/share/man/man8/sensors-detect.8" \
	"$PKG/usr/share/zsh/site-functions/_sensors-detect"
