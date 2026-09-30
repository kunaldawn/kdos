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

# A MEDIA PLAYER THAT NEEDS NO TOOLKIT. mpv draws with its own OpenGL/Vulkan
# renderer straight onto a Wayland surface — no GTK, no Qt, no widget set — so
# it plays video on the lightest machine and under llvmpipe, and libmpv is the
# engine the toolkit front ends (Haruna) draw over.
#
# -Dsixel=enabled is the terminal video output. The Wayland path needs a
# compositor, and `--vo=sixel` is how a video reaches a terminal that answered
# the DA reply — a serial line, an ssh login, or a terminal on tty2. Without it
# mpv there has no video output at all and plays the sound of a film.
#
# Wayland is the default output, and -Dx11=enabled is the second one: a front
# end that embeds mpv by window id (SMPlayer runs as an xcb client and passes
# --wid) needs an X11 window to draw into, and without one it plays the sound
# with no picture. egl-x11 is OpenGL on that window through Mesa's EGL, and
# x11-clipboard is the clipboard there. gl-x11 (GLX), xv and vdpau stay off, as
# does vaapi-x11, which needs a libva built with its X11 backend.
#
# -Dgl=enabled -Degl=enabled with egl-wayland, egl-drm, gbm and dmabuf-wayland
# is what makes the Wayland and KMS paths work at all; vaapi with its drm and
# wayland halves is hardware decode through libva.
#
# -Dlua=luajit is the scripting layer, and with it the on-screen controller,
# the stats overlay and the console: all three are Lua scripts built into the
# binary. mpv takes Lua 5.1 or 5.2 only, so the host's lua is no use to it;
# luajit is 5.1 with the 5.2 extensions its port enables.
#
# libplacebo is a HARD dependency of mpv 0.41 and is a port, built with both
# its Vulkan and its OpenGL backend. `--vo=gpu-next` — first in mpv's default
# order — runs on Vulkan where a device answers and on OpenGL through EGL
# where none does, so a GPU or a virtual machine with no Vulkan driver still
# gets the gpu-next renderer. `--vo=gpu`, mpv's own GL renderer, stays built.
#
# rubberband is the af=rubberband pitch and tempo filter, zimg the software
# scaler mpv prefers over libswscale for conversions and screenshots, jpeg the
# screenshot writer; dvbin needs only the kernel's DVB headers. uchardet is
# `--sub-codepage=auto`, the default: an external subtitle in CP1251, GBK or
# Shift-JIS is detected and converted rather than drawn as mojibake.
#
# The optical drive: libbluray is bd:// (an unencrypted disc or a backup, with
# no BD-J menus), dvdnav is dvd:// (titles and chapters; mpv has no DVD menus,
# which are GStreamer's rsndvdbin; libdvdread opens a CSS-encrypted disc
# through libdvdcss), and cdda is cdda:// through
# libcdio-paranoia. mpv builds the last two only as a GPL build, which this
# one is (-Dgpl defaults to true).
#
# Everything else is named off: no port (caca, vapoursynth, mujs,
# spirv-cross, cuda), a second shader compiler beside libplacebo's glslang
# (shaderc), a second route to the same sound server (pulse, jack, openal,
# sdl2), or a legacy one (oss-audio). build-date is off because a
# timestamp in the binary makes two builds of the same source differ.
#
# -Dcplugins=enabled is what loads mpv-mpris, the C plugin that puts mpv on the
# session bus for the media keys and the panel.
meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Db_ndebug=if-release \
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
