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

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-nls \
	--disable-rpath \
	--enable-multi-threading-support \
	--with-pthread \
	--with-libcerror=no \
	--with-libcthreads=no \
	--with-libcdata=no \
	--with-libclocale=no \
	--with-libcnotify=no \
	--with-libcsplit=no \
	--with-libuna=no \
	--with-libcfile=no \
	--with-libcpath=no

grep -q '^#define HAVE_MULTI_THREAD_SUPPORT 1' common/config.h

make
make DESTDIR=$PKG install
