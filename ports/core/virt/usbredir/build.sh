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

# -Dtools=enabled is usbredirect, the command that exports a local USB device
# to a remote QEMU; it is the one part that links glib.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dtools=enabled \
	-Dtests=disabled \
	-Dfuzzing=disabled \
	-Dgit_werror=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
