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

# INSTALL_UDEV_RULES=OFF because 52-airspyhf.rules grants plugdev, a group
# this system does not have; fs/etc/udev/rules.d/70-kdos-sdr.rules grants the
# device to dialout. The static archive is built unconditionally, so it is
# removed rather than shipped.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DINSTALL_UDEV_RULES=OFF \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install
rm "$PKG/usr/lib/libairspyhf.a"

test -x "$PKG/usr/bin/airspyhf_info"
test -f "$PKG/usr/lib/pkgconfig/libairspyhf.pc"
