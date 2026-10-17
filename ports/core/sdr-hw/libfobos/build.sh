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

mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DLIBVER=${version%%.git*}
make
make DESTDIR=$PKG install

rm -r "$PKG/usr/lib/udev"

test -f "$PKG/usr/lib/libfobos.so"
test -f "$PKG/usr/include/fobos.h"
test -x "$PKG/usr/bin/fobos_devinfo"
grep -q '^Version: .' "$PKG/usr/lib/pkgconfig/libfobos.pc"
