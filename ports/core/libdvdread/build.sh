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

# libdvdnav's base, and gst-plugins-ugly's dvdreadsrc. -Dlibdvdcss=disabled:
# libdvdcss is not a port, so a CSS-encrypted disc does not open; the library
# still tries to load libdvdcss.so.2 at run time, so one installed by hand is
# used.
#
# musl-off64_t.patch: the installed dvd_filesystem.h declares off64_t for
# glibc and the BSDs only, so on musl every consumer that includes sys/types.h
# before it fails to compile.
patch -p1 -i "$PORT_SRC/musl-off64_t.patch"
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Ddefault_library=shared \
	-Dlibdvdcss=disabled \
	-Denable_docs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
