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

meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Dlibmpv=true \
	-Dcplayer=true \
	-Dbuild-date=false \
	-Dcplugins=enabled \
	-Dx11=enabled \
	-Degl-x11=enabled \
	-Dgl-x11=disabled \
	-Dvaapi-x11=disabled \
	-Dvdpau=disabled \
	-Dvdpau-gl-x11=disabled \
	-Dxv=disabled \
	-Dx11-clipboard=enabled \
	-Dwayland=enabled \
	-Dgl=enabled \
	-Degl=enabled \
	-Degl-wayland=enabled \
	-Degl-drm=enabled \
	-Dgbm=enabled \
	-Ddmabuf-wayland=enabled \
	-Ddrm=enabled \
	-Dvulkan=enabled \
	-Dshaderc=disabled \
	-Dspirv-cross=disabled \
	-Dvaapi=enabled \
	-Dvaapi-drm=enabled \
	-Dvaapi-wayland=enabled \
	-Dcuda-hwaccel=disabled \
	-Dcuda-interop=disabled \
	-Dcaca=disabled \
	-Dsixel=enabled \
	-Dalsa=enabled \
	-Dpipewire=enabled \
	-Dpulse=disabled \
	-Djack=enabled \
	-Dopenal=disabled \
	-Dsndio=disabled \
	-Doss-audio=disabled \
	-Dsdl2-audio=disabled \
	-Dsdl2-video=disabled \
	-Dsdl2-gamepad=disabled \
	-Dlua=luajit \
	-Djavascript=disabled \
	-Drubberband=enabled \
	-Dzimg=enabled \
	-Djpeg=enabled \
	-Dzlib=enabled \
	-Diconv=enabled \
	-Duchardet=enabled \
	-Dlibavdevice=enabled \
	-Dlibarchive=enabled \
	-Dlcms2=enabled \
	-Dlibbluray=enabled \
	-Ddvdnav=enabled \
	-Dcdda=enabled \
	-Ddvbin=enabled \
	-Dvapoursynth=disabled \
	-Dmanpage-build=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
