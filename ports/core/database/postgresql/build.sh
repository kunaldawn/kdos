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
        if ! getent passwd postgres >/dev/null; then
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
