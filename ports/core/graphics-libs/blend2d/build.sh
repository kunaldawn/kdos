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

# AsmJit is compiled into the library from the copy in 3rdparty/asmjit, which
# the release tarball pins to the revision this release was tested with.
# BLEND2D_EXTERNAL_ASMJIT exists but upstream does not support it: asmjit
# publishes no releases, only a moving master.
mkdir -p build && cd build
cmake .. \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBLEND2D_STATIC=OFF \
	-DBLEND2D_TEST=OFF \
	-DBLEND2D_DEMOS=OFF \
	-DBLEND2D_EXTERNAL_ASMJIT=OFF
make
make DESTDIR=$PKG install
