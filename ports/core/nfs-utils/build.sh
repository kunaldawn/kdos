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

autoreconf -f -i

# --disable-nfsv3 REMOVES rpcbind ENTIRELY, and that is the point rather than a
# saving. NFSv3 needs a portmapper, a lock daemon and a status daemon — three
# more services on a machine with no systemd to sequence them — while NFSv4 is
# one port and one daemon. Between two Linux machines, which is the case this
# exists for, v4 is what you would choose anyway.
#
# samba is the interoperability answer and this is the CORRECTNESS one: SMB
# does not carry POSIX ownership, permissions, symlinks or byte-range locks the
# way a Linux program expects, so a git tree or a build directory over SMB
# behaves subtly wrong where over NFSv4 it does not.
# THREE glibc ASSUMPTIONS, and none of them is about NFS. rpcgen uses `struct stat64`
# and stat64(), the LFS64 spelling — musl keeps those as plain aliases but only
# behind _LARGEFILE64_SOURCE, and without it the struct has no known storage
# size on a target where the distinction has no meaning anyway. And getport.c
# calls offsetof() without <stddef.h> and nfsdctl.c calls basename() without
# <libgen.h>; glibc supplies both through headers it happens to pull in and
# musl does not. An implicit basename() is the worse of the two — it returns
# int, so the pointer is truncated to 32 bits rather than merely undeclared.
export CFLAGS="$CFLAGS -D_LARGEFILE64_SOURCE -include stddef.h -include libgen.h"

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--localstatedir=/var \
	--disable-nfsv3 \
	--enable-nfsv4 \
	--enable-nfsv41 \
	--disable-gss \
	--disable-ipv6 \
	--without-systemd \
	--with-statedir=/var/lib/nfs \
	--with-rpcgen=internal
make
make DESTDIR=$PKG install
