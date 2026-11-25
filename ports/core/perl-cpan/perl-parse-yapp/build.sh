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

perl Makefile.PL INSTALLDIRS=vendor
make
make DESTDIR=$PKG install
find "$PKG" -name .packlist -delete
find "$PKG" -name perllocal.pod -delete
find "$PKG" -type d -empty -delete

install -Dm644 "$SRC/README" "$PKG/usr/share/licenses/$name/README"
install -Dm644 "$SRC/lib/Parse/Yapp.pm" "$PKG/usr/share/licenses/$name/Yapp.pm"
install -Dm644 /usr/share/licenses/perl/Copying "$PKG/usr/share/licenses/$name/Copying"
