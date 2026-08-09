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

sed -i 's|\$(LN_S) --force --relative .*kmod|ln -sf ../../bin/kmod|' Makefile.in

CFLAGS="$CFLAGS -include libgen.h" \
./configure --prefix=/usr          \
            --bindir=/bin          \
            --sysconfdir=/etc      \
            --with-rootlibdir=/lib \
            --with-xz              \
            --with-zstd            \
            --with-zlib
make
make DESTDIR=$PKG install

mkdir -p $PKG/sbin
for target in depmod insmod lsmod modinfo modprobe rmmod; do
  ln -sfv ../bin/kmod $PKG/sbin/$target
done

ln -sfv kmod $PKG/bin/lsmod
