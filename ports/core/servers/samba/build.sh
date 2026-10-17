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

patch -p1 -i "$PORT_SRC/musl-netlink.patch"

export XSLTPROC="xsltproc --maxdepth 10000"

./configure \
	--prefix=/usr \
	--enable-fhs \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--localstatedir=/var \
	--with-piddir=/run/samba \
	--with-privatedir=/var/lib/samba/private \
	--with-logfilebase=/var/log/samba \
	--bundled-libraries='!talloc,!tdb,!tevent,ALL' \
	--without-ad-dc \
	--without-ads \
	--without-ldap \
	--without-pam \
	--without-systemd \
	--without-winbind \
	--with-acl-support \
	--without-quotas \
	--without-ldb-lmdb \
	--with-libunwind \
	--without-lttng \
	--without-fam \
	--without-dmapi \
	--without-winexe \
	--without-kernel-keyring \
	--with-regedit \
	--with-shared-modules='vfs_io_uring,!vfs_snapper' \
	--disable-spotlight \
	--disable-glusterfs \
	--disable-cephfs \
	--enable-cups \
	--disable-iprint \
	--enable-avahi \
	--disable-rpath \
	--disable-rpath-install \
	--nopyc --nopyo
grep -q 'define HAVE_CUPS 1' bin/default/include/config.h || {
	echo 'samba: libcups not found, printer shares would be missing' >&2
	exit 1
}
grep -q 'define HAVE_LINUX_RTNETLINK_H 1' bin/default/include/config.h
make
make DESTDIR=$PKG install
rm -rf "$PKG/run" "$PKG/var/run" "$PKG/var/lock"
PKG_CONFIG_PATH="$PKG/usr/lib/pkgconfig" pkg-config --exists ndr_standard samba-util smbclient

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/83_samba.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="samba"
DAEMON="/usr/sbin/smbd"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # A share nobody declared is a listening socket nobody asked for, so
        # smbd starts only once a non-empty smb.conf exists.
        if [ ! -s /etc/samba/smb.conf ]; then
            echo "[SKIP] $NAME: no /etc/samba/smb.conf"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON" --foreground --no-process-group
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/83_samba.sh"
