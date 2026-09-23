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
./configure \
	--prefix=/usr \
	--disable-nls \
	--disable-java \
	--disable-csharp \
	--without-git \
	--without-emacs \
	--with-included-libxml \
	--with-libunistring-prefix=/usr \
	--with-libncurses-prefix=/usr \
	--without-selinux \
	--without-libsmack
make
make DESTDIR=$PKG install

# --disable-java still installs the prebuilt javaversion.class that the Java
# probe in gettext's own m4 macros runs; nothing here runs Java.
rm -f "$PKG/usr/share/gettext/javaversion.class"

