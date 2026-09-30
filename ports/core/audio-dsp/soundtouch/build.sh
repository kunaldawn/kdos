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

# Upstream compiles with -ffast-math, which lets the compiler drop NaN and
# infinity handling and reorder float sums, so output would differ from the
# arithmetic the source states; the patch keeps -O3 alone.
patch -p1 -i "$PORT_SRC/no-fast.patch"

# OPENMP stays off, as upstream defaults it: it starts a thread pool inside
# every SoundTouch instance, under hosts that already run one per stream.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
	-DINTEGER_SAMPLES=OFF -DOPENMP=OFF -DNEON=OFF \
	-DSOUNDSTRETCH=ON -DSOUNDTOUCH_DLL=OFF
make
make DESTDIR=$PKG install
