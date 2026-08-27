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
# realm to join.
#
# --without-ldb-lmdb follows from --without-ad-dc: the lmdb backend exists for
# the domain controller's database, which is not built here, and lmdb is not a
# port. Samba makes it an ERROR rather than a downgrade — "ldb build (unless
# --without-ldb-lmdb) requires lmdb 0.9.16 or later" — so it has to be said.
#
# --without-libunwind, AND IT COSTS A STACK TRACE ON A CRASH. musl has no
# execinfo.h — backtrace() and backtrace_symbols() are a glibc extension — so
# samba's fault handler cannot print one, and configure stops rather than
# choosing for you. The alternative is the nongnu libunwind, which this tree
# does not have: `ports/core/libunwind` is LLVM's, and although it exports
# unw_getcontext/unw_init_local/unw_step (checked with nm) samba's probe wants
# the nongnu package and its pkg-config file, and does not find it. What is
# lost is samba's own backtrace; the kernel still writes a core.
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
	--without-acl-support \
	--without-quotas \
	--without-ldb-lmdb \
	--without-libunwind \
	--disable-cups \
	--disable-avahi \
	--disable-rpath \
	--disable-rpath-install \
	--nopyc --nopyo
make
make DESTDIR=$PKG install

# smbd is not supervised by default: a file server that starts on every boot on
# a laptop is a listening socket nobody asked for. The script is here so
# `ksvc start samba` is one command, and enabling it is a decision.
install -Dm755 /dev/stdin $PKG/etc/init.d/75_samba.sh <<'SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="samba"
DAEMON="/usr/sbin/smbd"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # A share nobody declared is a listening socket nobody asked for, so
        # this needs an smb.conf with at least one section beyond [global].
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
SH
