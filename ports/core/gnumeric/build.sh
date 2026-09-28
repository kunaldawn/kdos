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


# The Python plugin loader is built only when pygobject-3.0 is found, and the
# functions and plugins written in Python are absent without it. Psiconv,
# Paradox and libgda have no port, so those importers and the database plugin
# are off; the Perl loader carries no plugin anyone needs and is off with
# them. --disable-nls leaves out every translation: bundled data is English
# only. The GSettings schemas are compiled by kpkg's shared-index step, not
# at install into $PKG.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-nls \
	--disable-schemas-compile \
	--disable-introspection \
	--with-gtk \
	--with-python \
	--without-perl \
	--without-psiconv \
	--without-paradox \
	--without-gda
make
make DESTDIR=$PKG install
