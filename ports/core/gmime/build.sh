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

# A TAG ARCHIVE, so autotools has to run. --disable-introspection and no vala:
# both generate bindings for languages nothing here uses, and gobject-
# introspection would be a port carried for one consumer's optional feature.
#
# crypto stays ON. gmime's whole value over a hand-rolled MIME parser is that
# it verifies a signature and decrypts a part correctly, and a mail client that
# silently cannot is worse than one that says so.
# NOT ./autogen.sh: it walks a list of VERSIONED automake names — 1.9 through
# 1.16 — and gives up with "You must have automake >= 1.9.x installed" on a
# machine whose automake is newer than the list and unversioned. autoreconf
# uses whichever is installed. Same shape as minicom's aclocal-1.16.
# `touch ChangeLog` is autogen.sh's own line: automake requires the file to
# exist and a tag archive has no generated changelog.
#
# gtkdocize is what copies gtk-doc.make in, and it comes from the gtk-doc port
# — which is here for its m4 macro rather than for documentation. Stubbing
# that file instead does not work: GTK_DOC_CHECK is an m4 macro, so without
# gtk-doc's aclocal directory it survives into configure as a literal and the
# shell reports `syntax error near unexpected token '1.8'`.
touch ChangeLog
gtkdocize

autoreconf -fi

./configure --prefix=/usr --libdir=/usr/lib \
	--disable-static \
	--disable-introspection \
	--disable-vala \
	--disable-gtk-doc \
	--enable-crypto
make
make DESTDIR=$PKG install
