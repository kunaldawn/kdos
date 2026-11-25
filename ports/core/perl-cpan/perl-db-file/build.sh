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

export DB_FILE_INCLUDE=/usr/include DB_FILE_LIB=/usr/lib DB_FILE_NAME=-ldb
perl Makefile.PL INSTALLDIRS=vendor
make
make DESTDIR=$PKG install
find "$PKG" -name .packlist -delete
find "$PKG" -name perllocal.pod -delete
find "$PKG" -type d -empty -delete

install -Dm644 README "$PKG/usr/share/licenses/$name/README"
install -Dm644 /usr/share/licenses/perl/Copying "$PKG/usr/share/licenses/$name/Copying"
