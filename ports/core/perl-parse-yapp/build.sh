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

# A BUILD DEPENDENCY OF samba AND NOTHING ELSE. pidl — samba's IDL compiler,
# which generates the DCE/RPC marshalling code — is written against
# Parse::Yapp, and samba's configure stops at `perl module
# "Parse::Yapp::Driver" not found` rather than degrading.
perl Makefile.PL INSTALLDIRS=vendor
make
make DESTDIR=$PKG install
# MakeMaker's per-architecture bookkeeping; neither file belongs in a package.
find "$PKG" -name .packlist -delete
find "$PKG" -name perllocal.pod -delete
find "$PKG" -type d -empty -delete
