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

patch -p1 -i "$PORT_SRC/openssl-4.patch"

export OPENSSL_PREFIX=/usr PERL_MM_USE_DEFAULT=1
perl Makefile.PL INSTALLDIRS=vendor
make
make DESTDIR=$PKG install
find "$PKG" -name .packlist -delete
find "$PKG" -name perllocal.pod -delete
find "$PKG" -type d -empty -delete
