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

# -Dwith_xapian=true: an archive's full-text and title indexes are xapian
# databases, and libzim is what opens them. Built without it, search over an
# archive is unavailable to libkiwix and everything above it.
meson setup build \
	--prefix=/usr --libdir=lib --buildtype=release \
	-Dtests=false -Dexamples=false -Ddoc=false \
	-Dwith_xapian=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
