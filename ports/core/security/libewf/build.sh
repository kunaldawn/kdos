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

patch -p1 -i "$PORT_SRC/fuse3.patch"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--mandir=/usr/share/man \
	--disable-static \
	--disable-nls \
	--disable-rpath \
	--disable-python \
	--enable-multi-threading-support \
	--enable-openssl-evp-md \
	--enable-openssl-evp-cipher \
	--with-pthread \
	--with-zlib \
	--with-adler32=zlib \
	--with-bzip2 \
	--with-openssl \
	--with-libuuid=/usr \
	--with-libfuse \
	--with-libcerror=no \
	--with-libcthreads=no \
	--with-libcdata=no \
	--with-libcdatetime=no \
	--with-libclocale=no \
	--with-libcnotify=no \
	--with-libcsplit=no \
	--with-libuna=no \
	--with-libcfile=no \
	--with-libcpath=no \
	--with-libbfio=no \
	--with-libfcache=no \
	--with-libfdata=no \
	--with-libfdatetime=no \
	--with-libfguid=no \
	--with-libfvalue=no \
	--with-libhmac=no \
	--with-libcaes=no \
	--with-libodraw=no \
	--with-libsmdev=no \
	--with-libsmraw=no

for have in HAVE_PTHREAD HAVE_ZLIB HAVE_BZLIB HAVE_LIBCRYPTO HAVE_UUID_GENERATE_RANDOM HAVE_LIBFUSE3; do
	if ! grep -q "^#define $have 1" common/config.h; then
		echo "libewf: configure did not find $have" >&2
		exit 1
	fi
done

make
make DESTDIR=$PKG install

rm -f $PKG/usr/lib/*.la
