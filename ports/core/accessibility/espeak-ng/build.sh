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

./autogen.sh

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--with-mbrola=no \
	--with-sonic=no \
	--with-pcaudiolib=yes \
	RONN=no KRAMDOWN=no \
	ac_cv_header_valgrind_memcheck_h=no
make 'src_speak_ng_LDFLAGS=-lm $(PCAUDIOLIB_LIBS)'
make DESTDIR=$PKG 'src_speak_ng_LDFLAGS=-lm $(PCAUDIOLIB_LIBS)' install
