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

# The stream and generator libraries stay on: every import library's
# conversion tools (wpd2html, cdr2xhtml, vsd2svg and the rest) link them. The
# unit tests need cppunit, which the tree does not carry.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--enable-streams \
	--enable-generators \
	--disable-tests \
	--disable-fuzzers \
	--disable-werror \
	--without-docs
make
make DESTDIR=$PKG install
