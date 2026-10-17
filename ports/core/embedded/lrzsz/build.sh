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

export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch \
	-Wno-declaration-missing-parameter-type"
./configure --prefix=/usr --mandir=/usr/share/man --disable-nls
make
make -j1 DESTDIR=$PKG install
for a in rz rx rb; do ln -sf lrz "$PKG/usr/bin/$a"; done
for a in sz sx sb; do ln -sf lsz "$PKG/usr/bin/$a"; done
for a in rz rx rb; do ln -sf lrz.1 "$PKG/usr/share/man/man1/$a.1"; done
for a in sz sx sb; do ln -sf lsz.1 "$PKG/usr/share/man/man1/$a.1"; done
