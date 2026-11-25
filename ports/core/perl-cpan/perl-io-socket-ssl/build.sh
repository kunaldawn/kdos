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

install -d "$PKG/usr/share/licenses/$name"
sed -n '/^=head1 COPYRIGHT/,$p' lib/IO/Socket/SSL.pod > "$PKG/usr/share/licenses/$name/COPYRIGHT"
install -Dm644 /usr/share/licenses/perl/Copying "$PKG/usr/share/licenses/$name/Copying"
