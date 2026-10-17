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

patch -p1 -i "$PORT_SRC/python314-ast-str.patch"

patch -p1 -i "$PORT_SRC/cython-long.patch"
patch -p1 -i "$PORT_SRC/cython-dict-keys.patch"

export USE_SDL2=1 USE_X11=0 USE_WAYLAND=0 USE_EGL=0 USE_GSTREAMER=0 \
	USE_PANGOFT2=0 USE_MESAGL=0 USE_OPENGL_ES2=0

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	filetype .

rm -rf "$PKG/usr/share/kivy-examples"
