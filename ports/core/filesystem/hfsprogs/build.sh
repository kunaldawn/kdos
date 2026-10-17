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

patch -p1 -i "$PORT_SRC/fix-stdarg.patch"
patch -p1 -i "$PORT_SRC/musl-compat.patch"

make \
	CFLAGS="$CFLAGS -Wall -fblocks -I$PWD/BlocksRunTime -I$PWD/include -DDEBUG_BUILD=0 -D_FILE_OFFSET_BITS=64 -DLINUX=1 -DBSD=1 -DVERSION=\\\"$_upver\\\" '-D__P(x)=x'" \
	LDFLAGS="$LDFLAGS -Wl,--build-id -L$PWD/BlocksRunTime"

install -Dm755 fsck_hfs.tproj/fsck_hfs "$PKG/usr/sbin/fsck.hfsplus"
install -Dm755 newfs_hfs.tproj/newfs_hfs "$PKG/usr/sbin/mkfs.hfsplus"
ln -s fsck.hfsplus "$PKG/usr/sbin/fsck.hfs"
install -Dm644 fsck_hfs.tproj/fsck_hfs.8 "$PKG/usr/share/man/man8/fsck.hfsplus.8"
install -Dm644 newfs_hfs.tproj/newfs_hfs.8 "$PKG/usr/share/man/man8/mkfs.hfsplus.8"
