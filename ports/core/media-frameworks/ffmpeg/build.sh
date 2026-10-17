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

export CFLAGS="$CFLAGS -frandom-seed=ffmpeg"
./configure \
	--prefix=/usr \
	--mandir=/usr/share/man \
	--disable-static \
	--disable-debug \
	--disable-stripping \
	--enable-shared \
	--enable-pic \
	--enable-lto=auto \
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
	--enable-libbluray \
	--enable-libdvdnav \
	--enable-libdvdread \
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
	--enable-opencl \
	--enable-libtesseract \
	--enable-libqrencode \
	--enable-librsvg \
	--enable-libvidstab \
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
