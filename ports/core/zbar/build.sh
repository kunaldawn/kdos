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

# EVERY GUI FRONT END IS OFF. zbar ships GTK, Qt, Java and Python bindings and
# an X overlay; the host has none of those by rule, and what is wanted here is
# zbarimg and zbarcam — a file or a camera in, text out. `--with-x=no` matters
# even with the toolkits off, or configure links the X overlay for zbarcam.
# xmlto AND docbook-xsl: the man pages are built from docbook, and the
# stylesheet is named by its sourceforge URL. Only docbook-xsl's XML catalog
# rewrites that to the local copy — without it xsltproc tries to FETCH it and
# the build dies inside xmlto with an unresolved external entity. The catalog
# has to be NAMED: libxml2's compiled-in default is not what this build sees.
export XML_CATALOG_FILES=/etc/xml/catalog

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--without-gtk \
	--without-qt \
	--without-java \
	--without-python \
	--without-x \
	--with-imagemagick
make
make DESTDIR=$PKG install
