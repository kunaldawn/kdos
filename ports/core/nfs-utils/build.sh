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

# NFSv4 is the protocol this exists for: one port and one daemon, where NFSv3
# needs a portmapper, a lock daemon and a status daemon — three more services
# on a machine with no systemd to sequence them. configure has no switch that
# leaves the v3 helpers out, so they are built and simply never started.
# --enable-nfsv4server adds nfsv4.exportd, the export daemon a v4-only server
# runs in place of rpc.mountd, with no portmapper beside it.
#
# --enable-gss builds rpc.gssd, the client half of Kerberos NFS (sec=krb5*).
# It needs libtirpc built with its GSS-API layer and krb5's krb5-config.
# --disable-svcgss leaves out rpc.svcgssd, the server half, as upstream's
# default does. --disable-ldap: the umich_ldap idmap plugin wants openldap,
# which is not a port, and configure links it whenever it finds it.
# --enable-caps and --enable-uuid turn configure's silent probes for libcap and
# libblkid into requirements.
#
# samba is the interoperability answer and this is the CORRECTNESS one: SMB
# does not carry POSIX ownership, permissions, symlinks or byte-range locks the
# way a Linux program expects, so a git tree or a build directory over SMB
# behaves subtly wrong where over NFSv4 it does not.
# THREE glibc ASSUMPTIONS, and none of them is about NFS. rpcgen uses `struct stat64`
# and stat64(), the LFS64 spelling — musl keeps those as plain aliases but only
# behind _LARGEFILE64_SOURCE, and without it the struct has no known storage
# size on a target where the distinction has no meaning anyway. And fsidd.c
# calls offsetof() without <stddef.h> and nfsdctl.c calls basename() without
# <libgen.h>; glibc supplies both through headers it happens to pull in and
# musl does not. An implicit basename() is the worse of the two — it returns
# int, so the pointer is truncated to 32 bits rather than merely undeclared.
#
# -Wno-error=format: gssd's debug lines print a pthread_t with %lx, which is an
# integer on glibc and a pointer on musl, and configure promotes every format
# warning to an error. The value printed is the same width either way.
export CFLAGS="$CFLAGS -D_LARGEFILE64_SOURCE -include stddef.h -include libgen.h -Wno-error=format"

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--localstatedir=/var \
	--enable-nfsv4 \
	--enable-nfsv4server \
	--enable-gss \
	--disable-svcgss \
	--enable-ipv6 \
	--enable-caps \
	--enable-uuid \
	--disable-ldap \
	--without-systemd \
	--with-statedir=/var/lib/nfs \
	--with-rpcgen=internal
make
make DESTDIR=$PKG install
rm -f "$PKG/usr/share/man/man7/nfs.systemd.7"
