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

# THE RUNTIME HALF OF gettext, AND ONLY THAT. glibc answers gettext() from
# libc; musl does not, so a program written against GNU gettext resolves
# libintl_gettext or it does not link. The gettext port is built --disable-nls
# and installs no library, which is right for a system that ships no message
# catalogues — but it leaves the symbols with nothing behind them.
#
# Same tarball and same version as that port, so the header a caller compiles
# against and the library it links cannot describe two different gettexts.
#
# intl/ HAS ITS OWN configure AND IS NOT STANDALONE: its Makefile needs
# ../config.h, which only gettext-runtime's configure writes. So the runtime is
# configured and intl alone is built and installed.
#
# --with-included-gettext IS THE WHOLE POINT. musl carries gettext() in libc as
# an identity passthrough, so configure answers "GNU gettext in libc... yes",
# decides the system already has one and builds no library at all — an empty
# package, and `cannot find -lintl` at the first consumer. The header shipped
# by the gettext port maps every call to libintl_gettext, which musl does not
# define, so the two disagree and only the real library settles it.
cd gettext-runtime
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-rpath \
	--disable-java \
	--disable-csharp \
	--without-emacs \
	--enable-nls \
	--with-included-gettext
make -C intl
make -C intl DESTDIR=$PKG install

# The header is gettext's, installed by that package from this same tarball.
# Two packages owning one path is a file whose content depends on install
# order; the library is what this port exists to add.
rm -f "$PKG/usr/include/libintl.h"
