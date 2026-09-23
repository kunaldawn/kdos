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

# The tag archive carries no configure script; the loader's dispatch table is
# generated from ocl_interface.yaml by ruby at build time.
autoreconf -fi

# asciidoc, a2x and xmlto on PATH are what switch the libOpenCL(7) page on;
# configure disables it silently when any of the three is missing.
export XML_CATALOG_FILES=/etc/xml/catalog
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-official-khronos-headers \
	--disable-custom-vendordir \
	--disable-custom-layerdir \
	--enable-debug \
	--enable-pthread-once \
	--disable-update-database
make
make DESTDIR=$PKG install

# docdir holds the page's HTML rendering and the generated example bindings
# source; the package carries the manual page alone.
rm -rf "$PKG/usr/share/doc"
