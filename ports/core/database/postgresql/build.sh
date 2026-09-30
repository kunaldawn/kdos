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

# --with-icu IS THE ONE FLAG THAT CHANGES ANSWERS. musl's collation is
# essentially byte order, so a database built against it sorts and compares
# text differently from every other machine the data will touch — `ORDER BY`
# on anything but ASCII, `LIKE` and unique indexes on case-folded text all
# quietly disagree. ICU is already a port and is what makes those correct.
#
# sqlite is here and is right for one program's data; this is for the moment
# there are two, or two PEOPLE, and the file-locking answer stops working.
#
# --with-systemd is deliberately absent rather than disabled: it is off by
# default, and there is nothing here to integrate with.
#
# --with-system-tzdata makes time zones follow the tzdata port rather than a
# copy frozen into this build.
#
# --without-llvm: the JIT provider's version guards in 18.6 stop at LLVM 22,
# and the llvm port is 23. --with-ldap is the `ldap` method in pg_hba.conf,
# which checks a password against a directory server.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--datarootdir=/usr/share \
	--with-icu \
	--with-openssl \
	--with-readline \
	--with-libxml \
	--with-libxslt \
	--with-lz4 \
	--with-zstd \
	--with-python \
	--with-perl \
	--with-pam \
	--with-gssapi \
	--with-libcurl \
	--with-liburing \
	--with-uuid=e2fs \
	--with-system-tzdata=/usr/share/zoneinfo \
	--without-tcl \
	--with-ldap \
	--without-llvm \
	--disable-rpath
make world-bin
make -C doc/src/sgml man
make DESTDIR=$PKG install-world-bin
make -C doc/src/sgml DESTDIR=$PKG install-man

# ONE SHARED INSTANCE, UNDER ksvc, AS ITS ACCOUNT. postgres refuses to run as
# root and never drops privileges itself, so setpriv makes it the
# postinstall's `postgres` account before the exec; it never forks away, so
# the supervisor watches the server itself. The directory ships empty and the
# postinstall gives it to the account.
#
# IT SKIPS UNTIL THE CLUSTER EXISTS. initdb is the decision — locale,
# encoding, the superuser — and a server that made one on its own would have
# made it for somebody who never asked for a database:
#
#     sudo -u postgres initdb -D /var/lib/postgres/data
#
# ksvc stops with SIGTERM, which is postgres's SMART shutdown: it waits for
# connected clients. A client still connected at poweroff therefore ends in
# the final kill, and the next start replays the WAL — slower, never lossy.
install -d -m 700 "$PKG/var/lib/postgres"
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/76_postgresql.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="postgresql"
DAEMON="/usr/bin/postgres"
DATA="/var/lib/postgres/data"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^postgres:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no postgres account (the port's postinstall makes it)"
            exit 0
        fi
        if [ ! -s "$DATA/PG_VERSION" ]; then
            echo "[SKIP] $NAME: no cluster in $DATA (initdb makes one)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" setpriv --reuid=postgres --regid=postgres \
            --init-groups "$DAEMON" -D "$DATA"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/76_postgresql.sh"
