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

# libdvdnav's base, and gst-plugins-ugly's dvdreadsrc. -Dlibdvdcss=enabled
# links libdvdcss directly, so every consumer opens a CSS-encrypted disc; left
# to `auto` it would fall back to loading libdvdcss.so.2 at run time, and a
# missing one would be a disc that silently does not open.
#
# musl-off64_t.patch: the installed dvd_filesystem.h declares off64_t for
# glibc and the BSDs only, so on musl every consumer that includes sys/types.h
# before it fails to compile.
patch -p1 -i "$PORT_SRC/musl-off64_t.patch"
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Ddefault_library=shared \
	-Dlibdvdcss=enabled \
	-Denable_docs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
