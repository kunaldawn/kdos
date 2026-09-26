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

# The menu and chapter logic of a DVD, on top of libdvdread. mpv's dvd:// and
# gst-plugins-bad's resindvd link it.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Ddefault_library=shared \
	-Denable_docs=false \
	-Denable_examples=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
