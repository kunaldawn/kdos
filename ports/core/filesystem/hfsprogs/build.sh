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

# The makefile sets CC to clang: fsck_hfs is written with C blocks (-fblocks)
# and links the BlocksRunTime it builds, which gcc cannot compile. Its CFLAGS
# are -g3 with no optimisation level, and it ignores the exported flags, so
# CFLAGS and LDFLAGS are given on the command line: the exported flags, then
# every entry of the makefile's own except -g3 — the include paths, the
# defines and VERSION, which is _upver — and its own link flags. The top-level
# makefile exports both to the per-directory makefiles. The sources declare
# their functions through __P(), the K&R prototype macro of glibc's
# sys/cdefs.h; the sys/cdefs.h here is libbsd's, which has no __P, so it is
# defined on the command line, quoted for the shell make runs each compile in.
make \
	CFLAGS="$CFLAGS -Wall -fblocks -I$PWD/BlocksRunTime -I$PWD/include -DDEBUG_BUILD=0 -D_FILE_OFFSET_BITS=64 -DLINUX=1 -DBSD=1 -DVERSION=\\\"$_upver\\\" '-D__P(x)=x'" \
	LDFLAGS="$LDFLAGS -Wl,--build-id -L$PWD/BlocksRunTime"

# Installed under the names GParted and fsck(8) look up: fsck.hfsplus and
# mkfs.hfsplus, with fsck.hfs for plain HFS.
install -Dm755 fsck_hfs.tproj/fsck_hfs "$PKG/usr/sbin/fsck.hfsplus"
install -Dm755 newfs_hfs.tproj/newfs_hfs "$PKG/usr/sbin/mkfs.hfsplus"
ln -s fsck.hfsplus "$PKG/usr/sbin/fsck.hfs"
install -Dm644 fsck_hfs.tproj/fsck_hfs.8 "$PKG/usr/share/man/man8/fsck.hfsplus.8"
install -Dm644 newfs_hfs.tproj/newfs_hfs.8 "$PKG/usr/share/man/man8/mkfs.hfsplus.8"
