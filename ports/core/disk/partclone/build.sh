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

autoreconf -f -i

export XML_CATALOG_FILES=/etc/xml/catalog

MAN_XSL=$(ls -d /usr/share/xml/docbook/xsl-stylesheets-*/manpages/docbook.xsl 2>/dev/null | head -1)
if [ -z "$MAN_XSL" ]; then
	echo "ERROR: docbook-xsl manpages stylesheet not found under /usr/share/xml/docbook" >&2
	exit 1
fi
./configure \
	--prefix=/usr \
	--disable-rpath \
	--enable-extfs \
	--enable-fat \
	--enable-exfat \
	--enable-ntfs \
	--enable-hfsp \
	--enable-apfs \
	--enable-btrfs \
	--enable-xfs \
	--enable-f2fs \
	--enable-ncursesw \
	--enable-fuse
make MAN_STYLESHEET="$MAN_XSL"
make MAN_STYLESHEET="$MAN_XSL" DESTDIR=$PKG install
