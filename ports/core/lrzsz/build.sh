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

# A 1998 autoconf: its conftests are K&R and GCC 15 rejects every one of them,
# reporting the toolchain rather than the probe. Answer the whole family at once.
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch \
	-Wno-declaration-missing-parameter-type"
./configure --prefix=/usr --disable-nls
make
# -j1 ON THE INSTALL: install-exec-local hard-links lsz to lsb and declares no
# dependency on install-exec, which is what puts lsz there. In parallel the
# link runs first and fails on a file that does not exist yet.
make -j1 DESTDIR=$PKG install
# Upstream installs the tools under their build names; the commands people
# type are lrz/lsz and the rz/sz aliases every terminal program looks for.
for a in rz rx rb; do ln -sf lrz "$PKG/usr/bin/$a"; done
for a in sz sx sb; do ln -sf lsz "$PKG/usr/bin/$a"; done
