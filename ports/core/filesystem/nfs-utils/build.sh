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
	--with-rpcgen=internal \
	--disable-sbin-override
make
make DESTDIR=$PKG install
rm -f "$PKG/usr/share/man/man7/nfs.systemd.7" \
	"$PKG/usr/bin/rpcgen" \
	"$PKG/usr/share/man/man1/rpcgen.1" \
	"$PKG/var/lib/nfs/state"
test -x "$PKG/usr/sbin/mount.nfs"
test ! -e "$PKG/sbin"

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
