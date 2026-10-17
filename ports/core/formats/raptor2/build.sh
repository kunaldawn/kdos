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

patch -p1 -i "$PORT_SRC/libxml-2.11.0.patch"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--with-www=none \
	--enable-parsers="rdfxml ntriples turtle trig guess rss-tag-soup rdfa nquads json" \
	--enable-serializers="rdfxml rdfxml-abbrev turtle mkr ntriples rss-1.0 dot html json atom nquads" \
	--disable-gtk-doc
make
make DESTDIR=$PKG install

rm -rf "$PKG/usr/share/gtk-doc"
