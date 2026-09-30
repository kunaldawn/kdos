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

# THE i965 DRIVER — the VA-API entry point for the Intel iGPUs intel-media-driver
# does not serve: Sandy Bridge, Ivy Bridge and Haswell, which iHD refuses at
# init. libva maps the i915 kernel driver to iHD first and i965 second, so on a
# Broadwell or later machine iHD stays the one that loads and this driver is
# only reached where iHD declines. It installs into the dri directory libva's
# pkg-config names, which is where the dispatcher searches.
#
# The GPU shader kernels are the prebuilt .g*b binaries in src/shaders,
# included as headers; regenerating them needs intel-gen4asm, which is not a
# port.
#
# with_x11=no: the only X path is libva-x11, which libva here is built
# without. with_wayland=yes turns a missing libva-wayland into a setup error
# rather than a driver that silently cannot present to a Wayland surface.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	--wrap-mode=nodownload \
	-Dwith_x11=no \
	-Dwith_wayland=yes \
	-Denable_hybrid_codec=false \
	-Denable_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
