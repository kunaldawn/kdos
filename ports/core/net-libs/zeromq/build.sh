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

# libsodium supplies CURVE, the encryption a Jupyter kernel's connection file
# can ask for; without it the library still builds and refuses CURVE sockets at
# run time. Draft APIs stay off: pyzmq builds against the stable ABI, and a
# draft symbol one release adds is gone in the next. -Werror is upstream's
# default and would turn a newer compiler's warnings into failures. The
# release tarball carries built manual pages, which install as they are.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-Werror \
	--with-libsodium \
	--disable-drafts \
	--disable-perf \
	--without-pgm \
	--without-norm \
	--without-vmci \
	--without-tls \
	--without-nss \
	--without-libgssapi_krb5
make
make DESTDIR=$PKG install
