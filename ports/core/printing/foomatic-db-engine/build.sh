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

patch -p1 -i "$PORT_SRC/perl-defined-array.patch"

./configure --prefix=/usr --sysconfdir=/etc --libexecdir=/usr/lib \
	--disable-gscheck PERL_INSTALLDIRS=vendor LIB_CUPS=/usr/lib/cups
make
make DESTDIR=$PKG install
find "$PKG" -name .packlist -delete
find "$PKG" -name perllocal.pod -delete
find "$PKG" -type d -empty -delete
perl -I"$PKG/usr/share/perl5/vendor_perl" -e 'require Foomatic::PPD'
