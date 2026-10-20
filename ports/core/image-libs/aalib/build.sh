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
patch -p1 -i "$PORT_SRC/m4-quote.patch"
patch -p1 -i "$PORT_SRC/linuxkbd-bounds.patch"
patch -p1 -i "$PORT_SRC/config-no-rpath.patch"
patch -p1 -i "$PORT_SRC/ltconfig-musl.patch"

export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types \
	-Wno-return-mismatch -Wno-declaration-missing-parameter-type"

ac_cv_lib_gpm_Gpm_Repeat=no CC="$CC $LDFLAGS" \
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
readelf -d $PKG/usr/lib/libaa.so.1 | grep -q 'NEEDED.*\[libncursesw\?\.so'

rm -f $PKG/usr/share/info/dir
