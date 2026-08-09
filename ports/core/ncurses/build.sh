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

./configure --host=$TARGET --prefix=/usr \
            --mandir=/usr/share/man \
            --with-shared \
            --without-debug \
            --with-cxx-shared \
            --enable-pc-files \
            --enable-widec \
            --with-pkg-config-libdir=/usr/lib/pkgconfig
make
make DESTDIR=$PKG install

# Create non-wide compatibility links
for lib in ncurses form panel menu; do
	ln -sf lib${lib}w.so $PKG/usr/lib/lib${lib}.so
	ln -sf ${lib}w.pc $PKG/usr/lib/pkgconfig/${lib}.pc
done
ln -sf libncurses.so $PKG/usr/lib/libcurses.so
