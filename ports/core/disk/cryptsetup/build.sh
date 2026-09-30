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

# udev synchronisation follows the device-mapper library: --enable-udev asks for
# it, and it is lvm2's libdevmapper, built with udev_sync, that carries it. The
# tmpfiles.d directory is systemd's and is read from systemd.pc when one exists,
# so it is named off. Password-quality checking is off: libpwquality is not a
# port, and it would also be one more library for the initramfs list.
./configure \
	--prefix=/usr \
	--with-crypto_backend=openssl \
	--enable-udev \
	--enable-blkid \
	--disable-pwquality \
	--disable-passwdqc \
	--without-tmpfilesdir \
	--disable-asciidoc \
	--disable-ssh-token
make
make DESTDIR=$PKG install
