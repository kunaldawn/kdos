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

# The archive is a tag snapshot with no generated configure.
autoreconf -fi

# CHAP's MD5 comes from gnutls rather than the bundled copy. The conformance
# test tool and the unit tests need CUnit, which is not a port. The manual
# pages ship rendered; building them fetches a stylesheet from the network.
# iSER needs rdma-core, which is not a port, so only TCP transport is built.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-static \
	--disable-werror \
	--with-gnutls \
	--without-libgcrypt \
	--disable-test-tool \
	--disable-tests \
	--disable-examples \
	--disable-manpages
make
make DESTDIR=$PKG install

# Its page is installed with the others, for a tool that is not built.
rm -f "$PKG/usr/share/man/man1/iscsi-test-cu.1"
