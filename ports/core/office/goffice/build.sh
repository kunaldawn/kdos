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

# Embedded EPS is drawn through libspectre over libgs, a probe with no switch
# that drops EPS without a word when either is missing; the config header is
# checked for it after configure. MathML and itex equations are drawn
# through lasem, which --with-lasem=yes makes a hard requirement.
# Preferences go through GSettings; where they are stored is the session's
# GSETTINGS_BACKEND, not this library's.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--with-gtk \
	--with-librsvg \
	--with-lasem=yes \
	--with-config-backend=gsettings \
	--enable-introspection=yes \
	--disable-gtk-doc
grep -q '^#define GOFFICE_WITH_EPS 1' goffice/goffice-config.h
make
make DESTDIR=$PKG install
