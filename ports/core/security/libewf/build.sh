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


# The 20240506 release's configure finds fuse 3 and defines HAVE_LIBFUSE3, but
# ewfmount's sources test only HAVE_LIBFUSE and HAVE_LIBOSXFUSE: without the
# patch ewfmount links nothing from libfuse3 and answers every mount with "No
# sub system to mount EWF format". The patch adds the fuse 3 calling convention
# upstream's main branch uses — fuse_new before fuse_mount, the extra readdir
# and getattr arguments, fuse_unmount before fuse_destroy.
patch -p1 -i "$PORT_SRC/fuse3.patch"

# Every libyal m4 check treats a bare --with-<lib> as auto-detect: a library
# that is missing turns the feature off and configure still succeeds. So each
# optional library is asked for and config.h is checked afterwards — without
# HAVE_LIBFUSE3 there is no ewfmount, without zlib no compressed image opens,
# and without uuid_generate_random ewfacquire writes every image with an
# all-zero set identifier.
#
# --with-libuuid names a directory rather than auto-detecting: the pkg-config
# branch of this release's libuuid.m4 reads the wrong pkg-config variables and
# never checks <uuid/uuid.h> or its functions, so it reports libuuid found and
# links nothing. The directory branch runs the header and function checks.
#
# The bundled libyal libraries (libcerror, libbfio, libhmac and the rest) are
# none of them ports, so --with-<lib>=no pins each to the copy in the tarball.
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
