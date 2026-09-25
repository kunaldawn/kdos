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
# `--enable-gpl` is required by x264, x265 and rubberband, and it relicenses the
# resulting ffmpeg binary from LGPL-2.1+ to GPL-2+. Everything that links it
# inherits that: pipewire and gst-libav both do, and both are GPL-compatible.
# Removing x264, x265 and rubberband is the only way back to LGPL, and the
# first two cost H.264 and HEVC encode — the formats a phone records and a
# colleague expects. LICENSE.notice beside this file is the record, and it is
# what `kdos licence --audit` reads.
#
# The codec set is deliberately one encoder per format. dav1d decodes AV1 and
# svt-av1 encodes it; adding aom would be a second AV1 encoder for the same
# job, and libopenjpeg would be a second JPEG 2000 encoder beside the native
# one, so neither is enabled.
#
# Every autodetected library is named ON or OFF, so what ships does not depend
# on which ports happened to be installed first. sdl2 is OFF, and with it
# ffplay, because sdl2-compat reaches back to ffmpeg through sdl3 and pipewire:
# declaring it would be a dependency cycle. vdpau, sndio, CUDA/NVENC and AMF
# have no port, and vdpau's probe links libX11.
#
# --glslc=glslang picks the SPIR-V compiler the Vulkan filters, encoders and
# hwaccels are built with. Its probe only disables them when it fails, so the
# check after configure is what turns a missing compiler into an error.

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
	--enable-vaapi \
	--enable-vulkan \
	--glslc=glslang \
	--enable-v4l2-m2m \
	--enable-alsa \
	--enable-bzlib \
	--enable-zlib \
	--enable-lzma \
	--enable-iconv \
	--enable-libplacebo \
	--enable-lcms2 \
	--enable-liblc3 \
	--enable-librubberband \
	--enable-libjxl \
	--enable-libzimg \
	--enable-libsoxr \
	--enable-manpages \
	--disable-htmlpages \
	--disable-txtpages \
	--disable-sdl2 \
	--disable-vdpau \
	--disable-sndio \
	--disable-ffnvcodec \
	--disable-cuda-llvm \
	--disable-amf \
	--disable-libxcb \
	--disable-xlib
grep -q '^#define CONFIG_FFV1_VULKAN_ENCODER 1' config_components.h || {
	echo 'ffmpeg: no SPIR-V compiler was usable; the Vulkan filters are missing' >&2
	exit 1
}
make
make DESTDIR=$PKG install
