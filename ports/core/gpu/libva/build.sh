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

# Wayland and DRM only, no X11 or GLX backend. Consumers reach VA-API through
# a DRM render node or a Wayland display: ffmpeg through DRM, mpv with
# -Dvaapi-x11=disabled, kodi on its wayland platform, and chromium, whose
# VA-API wrapper loads libva and libva-drm alone. A program running under
# Xwayland that wants libva-x11 gets no hardware decode; the X11 backend
# would add libX11, libXext and libXfixes to this port's depends.
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
