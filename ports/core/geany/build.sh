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


# The terminal tab loads libvte-2.91 with dlopen at run time, so VTE is a
# run-time dependency the configure step never checks; without it the tab is
# absent. The HTML manual ships prebuilt in doc/ and is installed as it is;
# regenerating it, the PDF and the API reference needs docutils, rst2pdf and
# Doxygen for no change. --disable-nls leaves out every translation: bundled
# data is English only.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-nls \
	--enable-plugins \
	--enable-vte \
	--enable-socket \
	--disable-html-docs \
	--disable-pdf-docs \
	--disable-api-docs \
	--disable-gtkdoc-header
make
make DESTDIR=$PKG install
