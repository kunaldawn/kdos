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

# The XML-RPC library fldigi, flmsg and flamp link instead of each carrying
# their own copy.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static
make
make DESTDIR=$PKG install

test -e "$PKG/usr/lib/libflxmlrpc.so"
test -e "$PKG/usr/lib/pkgconfig/flxmlrpc.pc"
