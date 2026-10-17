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
	--disable-python \
	--enable-multi-threading-support \
	--with-pthread \
	--with-libbfio \
	--with-libfuse \
	--with-libcerror=no \
	--with-libcthreads=no \
	--with-libcdata=no \
	--with-libclocale=no \
	--with-libcnotify=no \
	--with-libcsplit=no \
	--with-libuna=no \
	--with-libcfile=no \
	--with-libcpath=no \
	--with-libfcache=no \
	--with-libfvalue=no \
	--with-zlib \
	--with-libfdata=no

for have in HAVE_MULTI_THREAD_SUPPORT HAVE_LIBBFIO HAVE_LIBFUSE3 HAVE_ZLIB; do
	if ! grep -q "^#define $have 1" common/config.h; then
		echo "libvmdk: configure did not find $have" >&2
		exit 1
	fi
done

make
make DESTDIR=$PKG install
