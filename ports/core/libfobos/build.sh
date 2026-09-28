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

# Upstream's branch at 2.4.1, not the 2.4.0 tag: the branch carries the
# device-open fix, the DC filter and the format-string fixes the tag lacks.
# SDRangel's Fobos plugin loads libfobos.so at run time and needs nothing
# of it at build time.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib
make
make DESTDIR=$PKG install

# The rule names a plugdev group this system does not have; device access
# comes from /etc/udev/rules.d/70-kdos-sdr.rules.
rm -r "$PKG/usr/lib/udev"

test -f "$PKG/usr/lib/libfobos.so"
test -f "$PKG/usr/include/fobos.h"
test -x "$PKG/usr/bin/fobos_devinfo"
