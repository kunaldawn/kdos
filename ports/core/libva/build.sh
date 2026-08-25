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

# Wayland and DRM only, no X11 backend.
#
# libva's x11 backend needs libX11, libXext and libXfixes; nothing on this host
# may pull in an X11 client chain for its own sake, and the only consumer here
# is ffmpeg, which reaches VA-API through DRM.
#
# The option spelling is not symmetric: DRM is `disable_drm`, a boolean
# defaulting to false, while the rest are `with_*` combos. meson rejects an
# unknown option outright, so a wrong guess fails the build rather than
# silently dropping the backend.
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dwith_x11=no \
	-Dwith_glx=no \
	-Dwith_win32=no \
	-Dwith_wayland=yes \
	-Ddisable_drm=false \
	-Denable_docs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
