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

# The API reference is built whenever doxygen is on the path. The probe takes
# a preset DOXYGEN only as an absolute path and otherwise searches again, so
# the answer goes in through its cache variable: false turns the
# documentation conditional off.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	ac_cv_path_DOXYGEN=false
make
make DESTDIR=$PKG install

test -f "$PKG/usr/lib/pkgconfig/libosmodsp.pc"
test -f "$PKG/usr/include/osmocom/dsp/iqbal.h"
