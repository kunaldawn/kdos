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

# --disable-http: opusfile's URL reader is an HTTP client inside an audio
# decoder, which is a network stack reached by opening a file. Nothing on this
# host wants it and every consumer here reads from disk. --disable-doc: the
# API reference is doxygen HTML only.
./configure --prefix=/usr --libdir=/usr/lib --disable-static --disable-http \
	--disable-doc --disable-examples
make
make DESTDIR=$PKG install
