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

# Upstream's fix for the x86 run-time dispatch: without it vec_avx.h does
# not find x86cpu.h and the AVX2 kernels do not compile.
patch -p1 -i "$PORT_SRC/rnnoise-fix-compilation-errors.patch"

# The release tarball carries the trained weights as src/rnnoise_data.c. A
# checkout from git has none and fetches them from a model server in
# autogen.sh, so the release tarball is the one that builds offline.
#
# On x86_64 the SSE4.1 and AVX2 kernels are chosen at run time; without the
# switch the library is compiled for the baseline alone.
case "$(uname -m)" in
	x86_64) _rtcd=--enable-x86-rtcd ;;
	*) _rtcd= ;;
esac
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-examples --disable-doc $_rtcd
make
make DESTDIR=$PKG install

test -f "$PKG/usr/lib/librnnoise.so"
test -f "$PKG/usr/lib/pkgconfig/rnnoise.pc"
