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

# `locate` AND `updatedb` ARE plocate's. findutils puts its own pair in
# /usr/bin, ahead of plocate's /usr/sbin/updatedb on $PATH, and neither reads
# the other's database or takes the other's options: kdos-updatedb's run fails
# on an unknown option and writes no index, and `locate` fails on the index
# LOCATE_PATH names. The locate directory still builds, because the top-level
# make descends into every subdirectory; the patch above is what lets it.
rm -f "$PKG/usr/bin/locate" "$PKG/usr/bin/updatedb" "$PKG/usr/libexec/frcode" \
	"$PKG/usr/share/man/man1/locate.1" "$PKG/usr/share/man/man1/updatedb.1" \
	"$PKG/usr/share/man/man5/locatedb.5"
rmdir "$PKG/usr/libexec" "$PKG/usr/share/man/man5"
rm -rf "$PKG/var"
