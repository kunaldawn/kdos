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

# THE ONE MEDIA PLAYER THAT NEEDS NO TOOLKIT, which is why it is a native port
# on a distro that boxes every other application. mpv draws with its own
# OpenGL/Vulkan renderer straight onto a Wayland surface — no GTK, no Qt, no
# widget set — so it is the only way to watch something without starting a
# container, and on a laptop that difference is a container's worth of memory
# and 18 seconds of cold start.
#
# -Dsixel=enabled is the terminal video output. The Wayland path needs a
# compositor, and `--vo=sixel` is how a video reaches a terminal that answered
# the DA reply — a serial line, an ssh login, or a terminal on tty2. Without it
# mpv there has no video output at all and plays the sound of a film.
#
# -Dx11=disabled is the hard rule, and every X11 sub-option (egl-x11, gl-x11,
# vaapi-x11, vdpau, xv, x11-clipboard) is named off with it so none can come
# back through a stray libX11 in the chroot. -Dgl=enabled -Degl=enabled with
# egl-wayland, egl-drm, gbm and dmabuf-wayland is what makes the Wayland and
# KMS paths work at all; vaapi with its drm and wayland halves is hardware
# decode through libva.
#
# -Dlua=luajit is the scripting layer, and with it the on-screen controller,
# the stats overlay and the console: all three are Lua scripts built into the
# binary. mpv takes Lua 5.1 or 5.2 only, so the host's lua is no use to it;
# luajit is 5.1 with the 5.2 extensions its port enables.
#
# libplacebo is a HARD dependency of mpv 0.41 and is a port. Its one GPU
# backend is Vulkan (its OpenGL backend needs glad2, which is not a port), so
# -Dvulkan=enabled is what gives `--vo=gpu-next` — first in mpv's default
# order — a context to run in. `--vo=gpu`, mpv's own GL renderer, is the
# fallback when no Vulkan device answers.
#
# rubberband is the af=rubberband pitch and tempo filter, zimg the software
# scaler mpv prefers over libswscale for conversions and screenshots, jpeg the
# screenshot writer; dvbin needs only the kernel's DVB headers. Everything else
# is named off: no port (uchardet, libbluray, dvdnav, cdda, caca, vapoursynth,
# mujs, shaderc, spirv-cross, cuda), a second route to the same sound server
# (pulse, jack, openal, sdl2), or a legacy one (oss-audio). build-date is off
# because a timestamp in the binary makes two builds of the same source differ.
meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Dlibmpv=true \
	-Dcplayer=true \
	-Dbuild-date=false \
	-Dcplugins=enabled \
	-Dx11=disabled \
	-Degl-x11=disabled \
	-Dgl-x11=disabled \
	-Dvaapi-x11=disabled \
	-Dvdpau=disabled \
	-Dvdpau-gl-x11=disabled \
	-Dxv=disabled \
	-Dx11-clipboard=disabled \
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
	-Djack=disabled \
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
	-Duchardet=disabled \
	-Dlibavdevice=enabled \
	-Dlibarchive=enabled \
	-Dlcms2=enabled \
	-Dlibbluray=disabled \
	-Ddvdnav=disabled \
	-Dcdda=disabled \
	-Ddvbin=enabled \
	-Dvapoursynth=disabled \
	-Dmanpage-build=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
