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

# The test suite builds a bundled Google Test and is not run here.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DDISABLE_TEST=ON \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

# The static archive is built by the same target list and has no consumer.
rm -f "$PKG/usr/lib/libmbe.a"

test -f "$PKG/usr/lib/libmbe.so.1"
test -f "$PKG/usr/include/mbelib.h"
