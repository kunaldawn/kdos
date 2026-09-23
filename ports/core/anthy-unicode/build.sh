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

# A tag archive, not a release tarball: upstream publishes no dist tarball, so
# there is no ./configure until autogen.sh has made one.
NOCONFIGURE=1 ./autogen.sh

# EMACS=no keeps AM_PATH_LISPDIR from probing for emacs, which is not a port;
# an emacs found on the build host would otherwise compile and install the
# elisp front end.
EMACS=no ./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--disable-static
make
make DESTDIR=$PKG install
