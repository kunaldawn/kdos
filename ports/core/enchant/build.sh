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

# Aspell is the one provider, with aspell-en as its dictionary. Every other
# provider is named off, so the set does not depend on what the chroot holds:
# hunspell, nuspell, hspell and voikko are not ports, and the rest are other
# operating systems'.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-static \
	--with-aspell \
	--without-hunspell \
	--without-nuspell \
	--without-hspell \
	--without-voikko \
	--without-winspell \
	--without-applespell \
	--without-zemberek
# nodist_doc_DATA is the HTML rendering of the three manual pages, made with
# `groff -Thtml`; groff is not a port, and the pages themselves install as
# pages. Emptied on the command line, where it reaches every subdirectory's
# make, the HTML is neither built nor installed.
make nodist_doc_DATA=
make DESTDIR=$PKG install nodist_doc_DATA=
