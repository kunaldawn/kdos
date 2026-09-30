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

# Alpine's musl patches: glibc's sys/sysctl.h and sys/errno.h do not exist here.
patch -p1 -i "$PORT_SRC/fix-stdarg.patch"
patch -p1 -i "$PORT_SRC/musl-compat.patch"

# The makefile sets CC to clang and its own CFLAGS: fsck_hfs is written with
# C blocks (-fblocks) and links the BlocksRunTime it builds, which gcc cannot
# compile. Flags passed on the command line would drop its include paths.
make

# Installed under the names GParted and fsck(8) look up: fsck.hfsplus and
# mkfs.hfsplus, with fsck.hfs for plain HFS.
install -Dm755 fsck_hfs.tproj/fsck_hfs "$PKG/usr/sbin/fsck.hfsplus"
install -Dm755 newfs_hfs.tproj/newfs_hfs "$PKG/usr/sbin/mkfs.hfsplus"
ln -s fsck.hfsplus "$PKG/usr/sbin/fsck.hfs"
install -Dm644 fsck_hfs.tproj/fsck_hfs.8 "$PKG/usr/share/man/man8/fsck.hfsplus.8"
install -Dm644 newfs_hfs.tproj/newfs_hfs.8 "$PKG/usr/share/man/man8/mkfs.hfsplus.8"
