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

cd unix

# IT IS HERE FOR yosys AND FOR NOTHING ELSE ON THIS HOST. yosys's command
# language is Tcl and its `tcl` command — which is how a synthesis script does
# anything conditional — needs a real interpreter linked in. There is no Tk and
# there will not be: that is a GUI toolkit and the hard rule covers it.
#
# --disable-static because the only consumer links the shared library, and
# a static libtcl in a synthesis binary is 4 MB nothing else can share.
#
# --with-system-libtommath IS WHAT MAKES yosys COMPILE. With tcl's own vendored
# copy, tclTomMath.h maps `mp_to_unsigned_bin` onto `TclBN_mp_to_unsigned_bin`
# — reachable only through tcl's stub table — so a consumer calling the plain
# names against tcl's PUBLIC headers fails on a symbol it never wrote, against
# a tcl that looks entirely healthy. The flag puts TCL_WITH_EXTERNAL_TOMMATH in
# tcl.pc, which is the string yosys's CMakeLists tests for before importing
# libtommath beside it.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--mandir=/usr/share/man \
	--enable-64bit \
	--with-system-libtommath \
	--disable-static
make
make DESTDIR=$PKG install

# The header set lands under a version directory that consumers do not look
# in; upstream's own install-private-headers is what puts the internal headers
# where a program embedding the interpreter finds them.
make DESTDIR=$PKG install-private-headers

ln -sf tclsh9.0 $PKG/usr/bin/tclsh
