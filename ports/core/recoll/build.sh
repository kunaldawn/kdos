#!/bin/bash


./autogen.sh

# CLI ONLY — --disable-qtgui, and there is no Qt on this host by rule. What
# remains is `recollindex` and `recollq`, which is the whole of what an offline
# machine needs: one index over the corpus, queried from a prompt or from any
# program that can read a list of paths.
#
# IT RIDES THE XAPIAN THE KIWIX STACK ALREADY NEEDS, which is what makes this
# cheap rather than a second search engine — the same library that gives a ZIM
# full-text search gives the local filesystem one.
#
# THE FILTERS SHELL OUT AND POPPLER IS WHY IT IS A DEPENDENCY. recoll indexes a
# PDF by running pdftotext; with no poppler it walks a directory of PDFs,
# reports success, and produces an index containing none of them. That is the
# failure this recipe's depends line exists to prevent, and it is silent.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-qtgui \
	--disable-webkit \
	--disable-python-chm \
	--without-systemd
make
make DESTDIR=$PKG install
