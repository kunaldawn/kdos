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

# with_INIReader=true builds the C++ half as a SECOND library, libINIReader.
# exiv2 requires it by name and finds nothing when it is off; the C consumers
# here (tio, imv, xfsprogs, xdg-desktop-portal-wlr) link libinih and are
# unaffected by its presence.
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Ddefault_library=shared \
	-Dwith_INIReader=true \
	-Ddistro_install=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
