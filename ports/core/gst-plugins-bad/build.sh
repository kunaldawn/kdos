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

# Four plugins are named explicitly, with `enabled` rather than meson's `auto`
# so that losing a library fails the build instead of producing a silently
# thinner package:
#
#   x265       HEVC encode          -Dgpl=enabled is required with it
#   svtav1     AV1 encode           (dav1d decodes, in -base's own auto set)
#   openjpeg   JPEG 2000
#   assrender  SSA/ASS subtitles over video
#
# aom is deliberately left off: svt-av1 is this tree's AV1 encoder, and a
# second encoder for one format earns nothing.

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Ddoc=disabled \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Dnls=disabled \
	-Dgpl=enabled \
	-Dx265=enabled \
	-Dsvtav1=enabled \
	-Dopenjpeg=enabled \
	-Dassrender=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
