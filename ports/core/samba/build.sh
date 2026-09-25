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

# --without-ad-dc HALVES IT, and the thing being given up is a domain
# controller nobody on an island network is going to run. What remains is the
# half that matters here: smbd serving a share, and the client libraries
# `mount.cifs` needs — because a Windows laptop, a phone or a network printer
# speaks SMB and frequently speaks nothing else.
#
# THE WAF BUILD SHELLS OUT TO python3 AT BUILD TIME and that is why python is a
# dependency of a C program. It also insists on writing into the source tree,
# so nothing here is out-of-tree.
#
# --bundled-libraries is the fiddly part: samba vendors talloc, tdb, tevent and
# ldb, none of which is a port here, so ALL is the honest setting — asking waf
# to find system copies that do not exist produces a configure failure several
# minutes in rather than at the first missing header.
#
# --without-systemd, --without-pam and --without-ads follow from what this
# distro is: no systemd, `authfw=shadow` rather than PAM, and no Kerberos
# realm to join. --without-ldap goes with --without-ads: samba's LDAP is the
# domain-member and ldapsam account backends, and openldap being on the image
# must not switch them on.
#
# --without-ldb-lmdb follows from --without-ad-dc: the lmdb backend exists for
# the domain controller's database, which is not built here, and lmdb is not a
# port. Samba makes it an ERROR rather than a downgrade — "ldb build (unless
# --without-ldb-lmdb) requires lmdb 0.9.16 or later" — so it has to be said.
#
# --with-libunwind IS SAMBA'S CRASH BACKTRACE. musl has no execinfo.h —
# backtrace() and backtrace_symbols() are a glibc extension — so without it
# the fault handler cannot print one, and configure stops rather than choosing
# for you. The probe wants libunwind-generic.pc and libunwind.h, which are
# libunwind-nongnu's; naming the option makes a missing one a configure error.
#
# --with-acl-support is what lets a Windows client change a file's permissions
# at all; without it the security tab is read-only. --enable-avahi advertises
# the share over mDNS, so a file manager on another machine finds it without a
# typed address. vfs_io_uring has no switch of its own: samba builds it
# whenever pkg-config finds liburing, so it is named in --with-shared-modules
# to make a missing liburing a build failure rather than a module that is
# silently not there. vfs_snapper is struck from the same list: it talks to
# snapperd over D-Bus, snapper is not a port, and left in the default list it
# makes dbus a configure-time requirement.
#
# --enable-cups lets a `[printers]` share hand Windows clients every queue the
# local CUPS has, which is what a KDOS machine sharing its printer needs. waf
# answers a missing libcups by building without printing rather than failing,
# so config.h is checked after configure. --disable-iprint drops Novell
# iPrint, which rides the same library and which nothing here serves.
#
# Everything else here that samba would otherwise decide by what happens to be
# installed is pinned. Spotlight is off: its only real backend queries an
# Elasticsearch server nothing here runs. GlusterFS, CephFS, FAM, DMAPI, LTTng
# and winexe's mingw cross-compiler are not ports. The kernel keyring stores
# Kerberos credentials and there is no realm to join. regedit is on because
# ncurses is a dependency.
#
# --enable-fhs IS REQUIRED WITH --prefix=/usr, and samba says so and then
# refuses: "Don't install directly under /usr or /usr/local without using the
# FHS option". Without it waf's default layout puts everything under
# /usr/{private,var,lib} in samba's own arrangement rather than the one the
# rest of this filesystem uses.
./configure \
	--prefix=/usr \
	--enable-fhs \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--localstatedir=/var \
	--with-piddir=/run/samba \
	--with-privatedir=/var/lib/samba/private \
	--with-logfilebase=/var/log/samba \
	--bundled-libraries=ALL \
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
make
make DESTDIR=$PKG install
rm -rf "$PKG/run" "$PKG/var/run" "$PKG/var/lock"
rm -f "$PKG/usr/share/man/man3/talloc.3"

# smbd is not supervised by default: a file server that starts on every boot on
# a laptop is a listening socket nobody asked for. The script is here so
# `ksvc start samba` is one command, and enabling it is a decision.
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
