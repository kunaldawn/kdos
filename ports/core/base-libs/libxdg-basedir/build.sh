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

# THE TARBALL SHIPS NO configure. It is a tag archive of the repository, so the
# autotools pass is run here rather than by upstream.
autoreconf -fi

# The Doxygen HTML is built only by an explicit target and never installed;
# --disable-doxygen-doc keeps configure from probing for doxygen at all.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-doxygen-doc
make
make DESTDIR=$PKG install
