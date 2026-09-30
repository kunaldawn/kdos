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

# libxml2 2.11 removed the entity's `checked` field that the entity loader
# sets; the patch drops the assignment, which libxml2 now tracks itself.
patch -p1 -i "$PORT_SRC/libxml-2.11.0.patch"

# No WWW library: file: URIs are read by raptor itself, and nothing here should
# fetch RDF from the network. GRDDL is left out with it, because it works by
# fetching the transforms a document names. ICU is the NFC checker.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--with-www=none \
	--enable-parsers="rdfxml ntriples turtle trig guess rss-tag-soup rdfa nquads json" \
	--enable-serializers="rdfxml rdfxml-abbrev turtle mkr ntriples rss-1.0 dot html json atom nquads" \
	--disable-gtk-doc
make
make DESTDIR=$PKG install

# The gtk-doc HTML reference ships prebuilt and installs whatever
# --disable-gtk-doc says; the package carries the manual pages only.
rm -rf "$PKG/usr/share/gtk-doc"
