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

# GnuTLS gives nbds:// and libxml2 parses NBD URIs.
# nbdsh is the Python binding's shell, so the Python binding stays on and
# installs into python3's site-packages. nbdublk needs liburing and ubdsrv,
# neither of them ported, and the OCaml, Go and Rust bindings have no
# consumer here.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--enable-fuse \
	--enable-python \
	--with-gnutls \
	--with-libxml2 \
	--disable-ublk \
	--disable-ocaml \
	--disable-golang \
	--disable-rust \
	--without-bash-completions \
	--disable-static
make
make DESTDIR=$PKG install
