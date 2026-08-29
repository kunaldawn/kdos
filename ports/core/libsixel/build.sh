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

# EVERY ONE OF THESE IS A `feature`, so the value is enabled/disabled/auto and
# never true/false — meson refuses a boolean here rather than coercing it. jpeg
# and png are named rather than left at `auto` because auto answers a missing
# dependency by disabling the feature, and a sixel encoder that cannot read a
# JPEG or a PNG is a library with no input.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dgd=disabled -Dtests=disabled -Djpeg=enabled -Dpng=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
