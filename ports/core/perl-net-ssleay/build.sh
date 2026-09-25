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

# Makefile.PL finds OpenSSL by running the `openssl` program under
# OPENSSL_PREFIX, and with no prefix it guesses among a list of directories;
# naming /usr pins it to the openssl port. PERL_MM_USE_DEFAULT answers its
# question about running tests over the network with the default, no, which a
# build with no network needs.
export OPENSSL_PREFIX=/usr PERL_MM_USE_DEFAULT=1
perl Makefile.PL INSTALLDIRS=vendor
make
make DESTDIR=$PKG install
# MakeMaker's per-architecture bookkeeping; neither file belongs in a package.
find "$PKG" -name .packlist -delete
find "$PKG" -name perllocal.pod -delete
find "$PKG" -type d -empty -delete
