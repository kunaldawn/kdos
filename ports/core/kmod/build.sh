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

# --without-openssl KEEPS libcrypto OUT OF THE INITRAMFS. configure adds it to
# the global LIBS, so libkmod links it as well as kmod, and the initramfs
# carries both by an explicit list that has libcrypto only beside cryptsetup:
# without it, modprobe and udev there fail to load and the overlay module never
# reaches the kernel. What
# is given up is modinfo's signer and sig_key for the PKCS#7 signatures the
# kernel writes.
CFLAGS="$CFLAGS -include libgen.h" \
./configure --prefix=/usr          \
            --bindir=/bin          \
            --sysconfdir=/etc      \
            --with-xz              \
            --with-zstd            \
            --with-zlib            \
            --without-openssl
make
make DESTDIR=$PKG install

mkdir -p $PKG/sbin
for target in depmod insmod lsmod modinfo modprobe rmmod; do
  ln -sfv ../bin/kmod $PKG/sbin/$target
done

ln -sfv kmod $PKG/bin/lsmod
