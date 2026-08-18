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

# The shipped binary is GPL-2-or-later, and that is a decision.
#
# `--enable-gpl` is required by x264 and x265, and it relicenses the resulting
# ffmpeg binary from LGPL-2.1+ to GPL-2+. Everything that links it inherits
# that: pipewire and gst-libav both do, and both are GPL-compatible. Removing
# x264/x265 is the only way back to LGPL, and it costs H.264 and HEVC encode —
# the formats a phone records and a colleague expects. LICENSE.notice beside
# this file is the record, and it is what `kdos licence --audit` reads.
#
# The codec set is deliberately one encoder per format. dav1d decodes AV1 and
# svt-av1 encodes it; adding aom would be a second AV1 encoder for the same
# job.

./configure \
	--prefix=/usr \
	--mandir=/usr/share/man \
	--disable-static \
	--disable-stripping \
	--enable-shared \
	--enable-pic \
	--enable-pthreads \
	--enable-version3 \
	--enable-gnutls \
	--enable-libdrm \
	--enable-libfontconfig \
	--enable-libfreetype \
	--enable-libfribidi \
	--enable-libharfbuzz \
	--enable-libwebp \
	--enable-libxml2 \
	--enable-gpl \
	--enable-libx264 \
	--enable-libx265 \
	--enable-libvpx \
	--enable-libsvtav1 \
	--enable-libdav1d \
	--enable-libopus \
	--enable-libvorbis \
	--enable-libmp3lame \
	--enable-libass \
	--enable-vaapi
make
make DESTDIR=$PKG install
