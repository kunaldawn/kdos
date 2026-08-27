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
# -Dx11=disabled is the hard rule; -Dgl=enabled -Degl=enabled is what makes the
# Wayland path work at all. -Dlua=disabled drops the scripting layer rather
# than adding a lua to the host for it.
#
# libplacebo is a HARD dependency of mpv 0.41 and is a port; ours is built with
# no GPU backend (its Vulkan-Headers and glad submodules are empty in a release
# archive), so `--vo=gpu-next` is unavailable and `--vo=gpu` — mpv's own GL
# renderer, what it shipped for years — is what plays video here.
meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Dlibmpv=true \
	-Dcplayer=true \
	-Dx11=disabled \
	-Dwayland=enabled \
	-Dgl=enabled \
	-Degl=enabled \
	-Ddrm=enabled \
	-Dvulkan=disabled \
	-Dalsa=enabled \
	-Dpipewire=enabled \
	-Dpulse=disabled \
	-Dsdl2-audio=disabled \
	-Dsdl2-video=disabled \
	-Dsdl2-gamepad=disabled \
	-Dlua=disabled \
	-Djavascript=disabled \
	-Dlibarchive=enabled \
	-Dlcms2=enabled \
	-Dmanpage-build=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
