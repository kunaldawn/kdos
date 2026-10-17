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

patch -p1 -i $PORT_SRC/updatedb-copyright-pipe.patch
./configure --prefix=/usr \
            --localstatedir=/var/lib/locate \
            --disable-nls \
            --without-selinux
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/bin/locate" "$PKG/usr/bin/updatedb" "$PKG/usr/libexec/frcode" \
	"$PKG/usr/share/man/man1/locate.1" "$PKG/usr/share/man/man1/updatedb.1" \
	"$PKG/usr/share/man/man5/locatedb.5"
rmdir "$PKG/usr/libexec" "$PKG/usr/share/man/man5"
rm -rf "$PKG/var"
