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

pci_opts=(
	PREFIX=/usr
	SHAREDIR=/usr/share/hwdata
	MANDIR=/usr/share/man
	SHARED=yes
	ZLIB=yes
	DNS=yes
	LIBKMOD=yes
	HWDB=yes
)

make OPT="${CFLAGS} -fPIC -DPIC" "${pci_opts[@]}"
make "${pci_opts[@]}" DESTDIR=$PKG install install-lib

chmod -v 755 $PKG/usr/lib/libpci.so
rm -f "$PKG/usr/share/hwdata/pci.ids.gz"
