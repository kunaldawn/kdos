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

# meson RATHER THAN cmake, because recoll asks for it by pkg-config and
# jsoncpp's meson build is the one that writes a jsoncpp.pc. Its cmake build
# writes a CMake config package instead, which recoll's `dependency('jsoncpp')`
# would find only through cmake's own lookup — a second route to the same
# library and one more thing to keep in step.
# NO -D FLAGS: jsoncpp ships no meson_options.txt at all, so every project
# option is a built-in and anything else fails at setup.
meson setup build --prefix=/usr --libdir=lib --buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
