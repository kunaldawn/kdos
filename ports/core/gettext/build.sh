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

# --with-included-libxml: libxml2's own closure reaches this port, so linking
# the system copy would be a depends cycle. libunistring, acl and attr are
# declared, so the system libunistring is always the one found and ACLs are
# always preserved; selinux and smack are no port and are pinned off.
#
# --with-xz fixes the format of autopoint's infrastructure archive. Left to
# probe, the format follows whichever of xz, git and bzip2 the build root
# holds, and autopoint then needs that same program on every machine it runs
# on; xz is declared, and it also gives the smallest archive.
./configure \
	--prefix=/usr \
	--disable-nls \
	--disable-java \
	--disable-csharp \
	--without-git \
	--with-xz \
	--without-emacs \
	--with-included-libxml \
	--with-libunistring-prefix=/usr \
	--with-libncurses-prefix=/usr \
	--without-selinux \
	--without-libsmack
make
make DESTDIR=$PKG install

# --disable-java still installs the prebuilt javaversion.class, which
# libgettextlib hands to a JVM to learn its Java version; the image carries no
# JVM, so nothing can run it.
rm -f "$PKG/usr/share/gettext/javaversion.class"

