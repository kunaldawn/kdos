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

# -Dwith_xapian=false here and true in libkiwix: libzim's own xapian use is the
# fulltext index INSIDE an archive, which libkiwix drives. Building it in both
# places links two copies of the same search engine into one process.
meson setup build \
	--prefix=/usr --libdir=lib --buildtype=release \
	-Dtests=false -Dexamples=false -Ddoc=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
