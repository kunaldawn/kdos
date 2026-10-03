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


# wvRTF passes a colour string to a printf-style call as its format; the
# patch gives it a "%s", or -Werror=format-security stops the build.
patch -p1 -i "$PORT_SRC/werrorformat.patch"

# libwmf has no port, so embedded Windows Metafile pictures are skipped
# when a document is converted. configure takes MKDIR_P from GNU or BusyBox
# mkdir only, and otherwise falls back to the bundled install-sh, which is
# X11R5's and has no -d: the install stops at the first directory it makes.
# toybox's mkdir -p is given instead.
MKDIR_P="mkdir -p" \
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--mandir=/usr/share/man \
	--disable-static \
	--with-zlib \
	--with-png \
	--without-libwmf
make
make DESTDIR=$PKG install
