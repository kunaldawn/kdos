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

# -std=gnu17: airspy.c declares its own `typedef int bool`, which C23 — gcc
# 15's default — rejects because bool is a keyword there.
#
# INSTALL_UDEV_RULES=OFF because 52-airspy.rules grants plugdev, a group this
# system does not have; fs/etc/udev/rules.d/70-kdos-sdr.rules grants the
# device to dialout. The static archive is built unconditionally, so it is
# removed rather than shipped.
export CFLAGS="$CFLAGS -std=gnu17"
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DINSTALL_UDEV_RULES=OFF
make
make DESTDIR=$PKG install
rm "$PKG/usr/lib/libairspy.a"
