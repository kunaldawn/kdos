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

# Upstream's official patch series, in order. Each is a diff from the top of
# the release tree, so -p0 from $SRC.
for p in "$_pfx"-[0-9][0-9][0-9]; do
	patch -p0 -i "$p"
done

./configure --host=$TARGET --prefix=/usr
make SHLIB_LIBS="-lncursesw"
make DESTDIR=$PKG install
