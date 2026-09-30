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

export CFLAGS="${CFLAGS/-O2/-O3}"

# --disable-examples: the example encoder and player are the only users of
# libvorbis and SDL, so without them the libraries need only libogg. The
# specification is LaTeX and the API reference doxygen, both off.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-examples --disable-spec --disable-doc
make
make DESTDIR=$PKG install
