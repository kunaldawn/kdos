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

# -Ddoc=false: the man pages are built with asciidoc and xmlto, neither of which
# is ported. The headers land in /usr/include/traceevent and the .pc says so, so
# consumers need no -I of their own.
meson setup build --prefix=/usr --libdir=lib --buildtype=release -Ddoc=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
