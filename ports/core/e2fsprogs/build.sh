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

# --disable-nls because MCONFIG puts $(LIBINTL) ahead of libsupport.a on every
# link line: once configure finds a libintl, the archive's gettext references
# come after the library that answers them and debugfs fails to link on
# libintl_gettext. No message catalogues ship, so nothing is lost.
mkdir -v build
cd build

../configure --prefix=/usr           \
             --bindir=/bin           \
             --with-root-prefix=""   \
             --enable-elf-shlibs     \
             --disable-libblkid      \
             --disable-libuuid       \
             --disable-uuidd         \
             --disable-fsck          \
             --disable-nls           \
             --enable-fuse2fs        \
             --with-libarchive       \
             --with-udev-rules-dir=/lib/udev/rules.d \
             --without-crond-dir     \
             --without-systemd-unit-dir
make
make DESTDIR=$PKG install
make DESTDIR=$PKG install-libs
