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

# Pure perl, and the ExifTool distribution is a MakeMaker one: INSTALLDIRS=vendor
# keeps the modules out of the perl core's own directory, which a perl upgrade
# replaces wholesale.
perl Makefile.PL INSTALLDIRS=vendor
make
make DESTDIR=$PKG install
# MakeMaker leaves a per-architecture .packlist and an empty perllocal.pod
# behind; both are build bookkeeping and neither belongs in a package.
find "$PKG" -name .packlist -delete
find "$PKG" -name perllocal.pod -delete
find "$PKG" -type d -empty -delete
