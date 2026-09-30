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

# Two options are needed for x264, not one.
#
# GStreamer refuses to build its GPL-licensed plugins unless the builder says
# so explicitly, so `-Dgpl=enabled` is required as well as `-Dx264=enabled`.
# With `-Dx264` alone — or with meson's `auto` — the plugin is skipped even
# though the library is installed, and the package builds and installs with
# nothing in it.
#
# `enabled` rather than `auto` throughout: this package exists for its codecs,
# so a missing library must fail the build rather than produce an empty
# package that `gst-inspect-1.0 x264enc` cannot explain.
#
# cdio is cdiocddasrc, an audio CD as a source (`cdda://`), and dvdread is
# dvdreadsrc, a DVD title without its menus; rsndvdbin in gst-plugins-bad is
# the one with menus. a52dec is AC-3 decode and mpeg2dec is MPEG-1/2 video
# decode, the two a DVD title needs. dvdread, a52dec and mpeg2dec are GPL and
# ride on -Dgpl=enabled with x264.

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Ddoc=disabled \
	-Dtests=disabled \
	-Dnls=disabled \
	-Dgpl=enabled \
	-Dx264=enabled \
	-Dorc=enabled \
	-Dorc-compiler=disabled \
	-Da52dec=enabled \
	-Dcdio=enabled \
	-Ddvdread=enabled \
	-Dmpeg2dec=enabled \
	-Dsidplay=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
