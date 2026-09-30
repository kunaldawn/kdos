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

patch -p1 -i $PORT_SRC/ncurses-opaque-window.patch
patch -p1 -i $PORT_SRC/curses-kbd-debug-print.patch

# aalib is from 2001 and ships an autoconf 2.13 configure whose very first
# probe is K&R: `main(){return(0);}`. GCC 14 promoted that whole family from
# warning to ERROR, so the compiler-works test fails and configure aborts with
# "C compiler cannot create executables" — a message that blames the toolchain
# for what is really its own conftest. The whole family is suppressed rather
# than the one that happened to fire first: each of the others costs another
# hour-long round trip to discover.
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types \
	-Wno-return-mismatch -Wno-declaration-missing-parameter-type"

# The slang driver stays off: a found slang disables the curses driver, and
# with --with-curses-driver=yes forcing both, slang is registered ahead of
# curses and becomes every consumer's terminal driver in its place.
#
# gpm is not a port and --with-gpm-mouse takes only a directory (=no would
# link -lgpm from "no/lib"), so the library probe itself is answered no.
ac_cv_lib_gpm_Gpm_Repeat=no \
./configure --prefix=/usr \
            --libdir=/usr/lib \
            --mandir=/usr/share/man \
            --infodir=/usr/share/info \
            --disable-static \
            --with-ncurses \
            --with-slang-driver=no \
            --with-x11-driver=no \
            --without-x
make
make DESTDIR=$PKG install

rm -f $PKG/usr/share/info/dir
