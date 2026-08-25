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
# driver at boot, so what is wanted here is the READER. PROG_EXTRA is empty
# for that reason.
#
# There is no configure. The makefile takes PREFIX/MANDIR and DESTDIR only,
# and ETCDIR must be passed to both stages or the library looks for
# sensors.conf under a prefix that is not where it was installed.
make PREFIX=/usr LIBDIR=/usr/lib ETCDIR=/etc PROG_EXTRA=
make PREFIX=/usr LIBDIR=/usr/lib ETCDIR=/etc PROG_EXTRA= DESTDIR=$PKG install
