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


# libdvdread links this when it is configured with -Dlibdvdcss=enabled, and
# only then does a commercial disc open in mpv's dvd:// or a ripper; without
# it a CSS-encrypted title reads as scrambled sectors. Title keys are cached
# per user in ~/.dvdcss (DVDCSS_CACHE=off turns that off), never system-wide.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Ddefault_library=shared \
	-Denable_docs=false \
	-Denable_examples=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
