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

# Every probe is pinned: configure answers a missing header or pkg-config
# module by dropping the feature. ZLIB=yes makes the ids file pci.ids.gz,
# LIBKMOD=yes (lspci -k) is fatal when pkg-config cannot find libkmod, HWDB=yes
# takes names from eudev's hwdb, and DNS=yes is `lspci -q`, which reaches the
# network only when asked. Both make lines take the same list, so the install
# step cannot configure a different library from the one that was built.
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
