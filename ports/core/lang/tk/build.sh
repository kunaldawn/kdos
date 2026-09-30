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

# TK HAS ONE WINDOWING SYSTEM ON LINUX, X11, so every Tk window on this desktop
# is an Xwayland client framed by kdos-comp.
#
# --enable-xft is what makes the text anti-aliased and fontconfig-matched;
# without it Tk asks the X server for core fonts, and Xwayland has no font
# path to serve them from.
#
# --disable-xss: the XScreenSaver extension library is not ported, and all it
# backs is `tk inactive`, which then reports -1.
#
# --enable-libcups backs `tk print` on this image's CUPS; configure finds it
# through cups-config.
#
# --disable-zipfs installs the Tk script library as files under /usr/lib/tk9.0
# rather than as a zip archive appended to the shared library, so a tool that
# rewrites the ELF file cannot silently cut the library off and leave a Tk
# that cannot find tk.tcl.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--mandir=/usr/share/man \
	--with-tcl=/usr/lib \
	--with-x \
	--enable-64bit \
	--enable-shared \
	--disable-rpath \
	--enable-xft \
	--disable-xss \
	--enable-libcups \
	--disable-zipfs
make
make DESTDIR=$PKG install

# The internal headers are what an extension built against Tk's private API
# looks for beside tk.h.
make DESTDIR=$PKG install-private-headers

ln -sf wish9.0 $PKG/usr/bin/wish
