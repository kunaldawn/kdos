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
# DETACH_KERNEL_DRIVER because the DVB-T driver claims the device on plug-in;
# fs/etc/modprobe.d/kdos-sdr.conf blacklists it and this is the second half —
# a dongle already claimed by a running kernel driver is otherwise unopenable.
# INSTALL_UDEV_RULES=OFF because rtl-sdr.rules grants a plugdev group this
# system does not have; fs/etc/udev/rules.d/70-kdos-sdr.rules carries the same
# ids at dialout, and a stick added to librtlsdr's table needs its line there.
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DDETACH_KERNEL_DRIVER=ON -DINSTALL_UDEV_RULES=OFF
make
make DESTDIR=$PKG install
install -Dm644 ../debian/rtl_*.1 -t $PKG/usr/share/man/man1
