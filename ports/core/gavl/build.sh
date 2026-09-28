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

# benchmark.c uses the CPU-affinity calls without asking for them, and musl
# only declares them under _GNU_SOURCE.
patch -p1 -i "$PORT_SRC/musl-sched_h.patch"

# The test programs built beside the library call memset without its header,
# which GCC 14 and later reject. The whole family is suppressed so the next
# one does not cost another round trip.
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types -Wno-return-mismatch \
	-Wno-declaration-missing-parameter-type"

# --with-cpuflags=none: the default reads /proc/cpuinfo and compiles for the
# builder's processor, so the library would fault on an older one. The SIMD
# paths are chosen at run time either way.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--mandir=/usr/share/man \
	--disable-static \
	--without-doxygen \
	--with-cpuflags=none
make
make DESTDIR=$PKG install
