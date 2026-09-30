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

# Thai is written without spaces between words, so a line can only be broken
# at a word boundary found in a dictionary. pango asks this library for them;
# without it a Thai paragraph wraps in the middle of a word.
#
# The dictionary is generated here, by libdatrie's trietool, from the word list
# in data/; --disable-dict would ship the library with nothing to look up.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-doxygen-doc
make
make DESTDIR=$PKG install
