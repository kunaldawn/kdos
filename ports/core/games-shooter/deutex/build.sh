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

tar -xf "$PORT_SRC/$name-$version.tar.zst" --strip-components=1

# PNG support is what the Freedoom build checks for; without it every graphic
# lump is refused. a2x resolves docbook-xsl's stylesheet only through the XML
# catalog, and without it the manual page build exits 5.
export XML_CATALOG_FILES=/etc/xml/catalog
./configure --prefix=/usr --mandir=/usr/share/man --with-libpng --enable-man
make
make DESTDIR=$PKG install
