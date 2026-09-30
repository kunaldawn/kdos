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
#
# --enable-nfsv4server adds nfsv4.exportd, the export daemon a v4-only server
# runs in place of rpc.mountd, with no portmapper beside it.
#
# --enable-gss builds rpc.gssd, the client half of Kerberos NFS (sec=krb5*).
# It needs libtirpc built with its GSS-API layer and krb5's krb5-config.
# --disable-svcgss leaves out rpc.svcgssd, the server half, as upstream's
# default does. --enable-ldap builds the umich_ldap idmap plugin, which maps
# NFSv4 owner names through a directory server when idmapd.conf names it.
# --enable-caps makes a missing <sys/capability.h> stop configure, and
# --enable-uuid a missing libblkid; left to default, both drop out in silence.
#
# nfsv4.exportd links only while the build optimises: exportd.c defines
# cleanup_lockfiles as a plain C99 `inline` with no external definition, so a
# call the compiler does not inline — as at -O0 — is an undefined reference.
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
	--enable-ldap \
	--without-systemd \
	--with-statedir=/var/lib/nfs \
	--with-rpcgen=internal
make
make DESTDIR=$PKG install
rm -f "$PKG/usr/share/man/man7/nfs.systemd.7"

# THE SERVER STARTS ONCE /etc/exports NAMES A SHARE. An export nobody declared
# is a listening socket nobody asked for, and a comment is not a share. It is
# NFSv4 alone: nfsv4.exportd answers the kernel's export upcalls where
# rpc.mountd and a portmapper would for v3, nfsdcld keeps the client records
# the kernel reclaims after a reboot — this kernel builds no other tracker, so
# without it every client loses its locks when the server restarts — and
# rpc.nfsd starts the kernel threads with v3 turned off. The kernel does the
# serving; rpc.nfsd exits, so only the two helpers are supervised.
#
# rpc_pipefs is where both the server's nfsdcld and the client's rpc.gssd meet
# the kernel, so each script mounts it if the other has not.
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/72_nfsd.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="nfsd"
DAEMON="/usr/sbin/rpc.nfsd"
EXPORTFS="/usr/sbin/exportfs"
PIPEFS="/var/lib/nfs/rpc_pipefs"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if ! grep -qEv '^[[:space:]]*(#|$)' /etc/exports 2>/dev/null; then
            echo "[SKIP] $NAME: no share in /etc/exports"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        modprobe nfsd 2>/dev/null
        grep -q " /proc/fs/nfsd nfsd " /proc/mounts || \
            mount -t nfsd nfsd /proc/fs/nfsd
        mkdir -p "$PIPEFS" /var/lib/nfs/nfsdcld
        grep -q " $PIPEFS rpc_pipefs " /proc/mounts || \
            mount -t rpc_pipefs sunrpc "$PIPEFS"
        # -F and -f: each stays in the foreground for ksvc.
        supervise nfsdcld /usr/sbin/nfsdcld -F
        "$EXPORTFS" -r
        supervise nfsv4.exportd /usr/sbin/nfsv4.exportd -f
        "$DAEMON" -N 3 -V 4
        ;;
    stop)
        # Threads first, so no request arrives to an export being withdrawn.
        "$DAEMON" 0 2>/dev/null
        "$EXPORTFS" -ua 2>/dev/null
        stop_service nfsv4.exportd
        stop_service nfsdcld
        ;;
    status)
        check_status nfsv4.exportd
        ;;
    *)  echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/72_nfsd.sh"

# rpc.gssd is the CLIENT's Kerberos half: the kernel asks it, through
# rpc_pipefs, for a context before a sec=krb5 mount can do anything. It starts
# once /etc/krb5.keytab exists, because with no machine key there is no
# credential for it to hand over.
cat > "$PKG/etc/init.d/63_gssd.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="gssd"
DAEMON="/usr/sbin/rpc.gssd"
PIPEFS="/var/lib/nfs/rpc_pipefs"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if [ ! -s /etc/krb5.keytab ]; then
            echo "[SKIP] $NAME: no /etc/krb5.keytab"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # sunrpc registers rpc_pipefs; rpcsec_gss_krb5 is what the kernel
        # asks for when a sec=krb5 mount arrives.
        modprobe sunrpc 2>/dev/null
        modprobe rpcsec_gss_krb5 2>/dev/null
        mkdir -p "$PIPEFS"
        grep -q " $PIPEFS rpc_pipefs " /proc/mounts || \
            mount -t rpc_pipefs sunrpc "$PIPEFS"
        # -f keeps it in the foreground for ksvc.
        supervise "$NAME" "$DAEMON" -f
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/63_gssd.sh"
