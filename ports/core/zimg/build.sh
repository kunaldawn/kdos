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

# The tag archive carries configure.ac and no configure. The unit tests need
# the googletest submodule the archive leaves empty, and --enable-unit-test
# also builds the library without fast-math, so it stays off.
./autogen.sh
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-simd \
	--disable-testapp \
	--disable-example \
	--disable-unit-test \
	--disable-debug
make
make DESTDIR=$PKG install
