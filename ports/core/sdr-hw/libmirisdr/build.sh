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

patch -p1 -i "$PORT_SRC/pc-version.patch"

mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DLIB_SUFFIX= \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DDETACH_KERNEL_DRIVER=ON \
	-DMIRISDR_PC_VERSION=$version
make
make DESTDIR=$PKG install
rm "$PKG/usr/lib/libmirisdr.a"

test -f "$PKG/usr/lib/libmirisdr.so"
test -f "$PKG/usr/include/mirisdr.h"
test -x "$PKG/usr/bin/miri_sdr"
grep -q '^Version: .' "$PKG/usr/lib/pkgconfig/libmirisdr.pc"
