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

patch -p1 -i "$PORT_SRC/boost-1.89.patch"
autoreconf -fi

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-boost=/usr \
	boost_cv_rpath_link_ldflag=-L \
	--enable-threads \
	--enable-python \
	--disable-vulkan \
	--disable-debug-utils
make
make DESTDIR=$PKG install
