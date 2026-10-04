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


# ast.Str is gone in python 3.14, and the kv-language parser's name walk
# over f-strings and and/or expressions still tests for it: every kv rule
# holding one raises AttributeError on load. The value of each part is what 3.8 and later hand
# over, so the patch walks that alone. Sideband checks for exactly this text
# at start-up and rewrites the installed file itself when it is missing,
# which on a read-only system directory ends the program.
patch -p1 -i "$PORT_SRC/python314-ast-str.patch"

# Cython 3.1 and later reject two Python 2 leftovers: the long builtin, which
# weakproxy.pyx's __long__ and two isinstance checks in the graphics modules
# still name, and RenderContext.apply's PY2 branch, which assigns a
# dict_keys to a list. The patches are upstream's removals (5a1b27d7 and the
# matching part of dcd8fb2a), rewritten for the release archive's CRLF line
# endings.
patch -p1 -i "$PORT_SRC/cython-long.patch"
patch -p1 -i "$PORT_SRC/cython-dict-keys.patch"

# Kivy draws everything itself with OpenGL in one SDL2 window. SDL2 here is
# sdl2-compat over SDL3, built without an X11 video driver, so the window is a
# Wayland surface; Kivy's own X11 and EGL window providers are not built, and neither is the GStreamer audio and video
# provider (SDL2_mixer plays sound). Pango text is off: SDL2_ttf is the text
# provider. Each switch is set, because setup.py otherwise decides by what
# pkg-config happens to find in the build root.
export USE_SDL2=1 USE_X11=0 USE_WAYLAND=0 USE_EGL=0 USE_GSTREAMER=0 \
	USE_PANGOFT2=0 USE_MESAGL=0 USE_OPENGL_ES2=0

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# BUILD ISOLATION IS OFF, so pyproject's pins (Cython at most 3.0.11,
# setuptools 69) are not enforced and the Cython and setuptools ports build
# it: Cython 3.0 predates python 3.14 and its generated C does not follow
# 3.14's API. setup.py prints a warning for the newer Cython and carries on.
# The bundle carries the pinned backends only because the fetch collects
# every declared build requirement; none of them is installed.
# filetype is imported by kivy.core.image and nothing else here imports it,
# so it comes from the bundle.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	filetype .

# The examples are a second copy of the documentation's code, installed as
# data under share/; nothing imports them.
rm -rf "$PKG/usr/share/kivy-examples"
