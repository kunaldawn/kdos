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

# The patch adds the <cstddef> includes current GCC no longer pulls in
# transitively; without it two sources fail on an undeclared size_t.
patch -p1 -i "$PORT_SRC/libwpd-gcc11.patch"
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--enable-tools \
	--disable-fuzzers \
	--disable-werror \
	--without-docs
make
make DESTDIR=$PKG install
