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

# foomatic-ppdfile is linked into cups' driver directory, so cups-driverd lists
# every foomatic-db printer and generates its PPD on demand; the Perl modules
# go to the vendor tree like every other Perl port's.
# --disable-gscheck: the check runs gs from the build root and changes nothing
# the package installs.
./configure --prefix=/usr --sysconfdir=/etc --libexecdir=/usr/lib \
	--disable-gscheck PERL_INSTALLDIRS=vendor
make
make DESTDIR=$PKG install
find "$PKG" -name .packlist -delete
find "$PKG" -name perllocal.pod -delete
find "$PKG" -type d -empty -delete
