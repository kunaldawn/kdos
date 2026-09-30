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

# Hunspell and Aspell are the providers: hunspell reads hunspell-en's
# dictionaries from /usr/share/hunspell, and aspell reads aspell-en. Both are
# named with --with, which makes a provider whose library is missing a failed
# configure rather than a quietly smaller set. Every other provider is named
# off, so the set does not depend on what the chroot holds: nuspell, hspell and
# voikko are not ports, and the rest are other operating systems'.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-static \
	--with-aspell \
	--with-hunspell \
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
