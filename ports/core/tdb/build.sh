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

# The trivial database Rhythmbox keeps its metadata cache in. Samba carries
# its own private copy (--bundled-libraries=ALL there) and installs the
# tdbtool, tdbdump, tdbbackup and tdbrestore programs and their pages from it,
# so this package installs the library, its header and tdb.pc only: the same
# four programs from both packages would be a file conflict. The Python
# binding is off for the same reason, samba installing its own.
#
# waf runs python3 at build time and writes into the source tree.
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
	--disable-python \
	--disable-rpath \
	--disable-rpath-install \
	--builtin-libraries=replace
make
make DESTDIR=$PKG install

rm -f "$PKG"/usr/bin/tdbtool "$PKG"/usr/bin/tdbdump \
	"$PKG"/usr/bin/tdbbackup "$PKG"/usr/bin/tdbrestore
rm -rf "$PKG/usr/share/man/man8"
rmdir "$PKG/usr/bin" 2>/dev/null || true
