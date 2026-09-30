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

# Both switches are recorded in the installed binio.h (BINIO_ENABLE_IOSTREAM,
# BINIO_ENABLE_STRING), so what is built here is the API every consumer
# compiles against. They are named rather than defaulted: configure turns
# either off quietly when its libstdc++ probe fails.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-iostream \
	--enable-string
make
make DESTDIR=$PKG install
