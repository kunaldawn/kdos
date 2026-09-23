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

# THE J-LINK TRANSPORT FOR flashrom AND openocd. The USB transport is pinned on:
# with -Dusb=auto a chroot missing libusb builds a library that only finds
# probes over TCP, and both consumers then report no J-Link attached.
#
# Upstream defaults to werror=true over warning_level=3 (-Wpedantic -Wextra);
# a warning new in the chroot's compiler would otherwise fail the build.
#
# contrib/60-libjaylink.rules is not installed and meson does not install it.
# Access to vendor 1366 is granted by fs/etc/udev/rules.d/70-kdos-debug.rules;
# upstream's file names a plugdev group this system does not have.
#
# The library has no manual pages; its Doxygen reference is not built.
meson setup build \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=lib \
	--buildtype=release \
	-Dwerror=false \
	-Dusb=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
