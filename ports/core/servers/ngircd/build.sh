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

./configure --prefix=/usr --sysconfdir=/etc \
	--with-openssl --without-gnutls \
	--with-zlib --with-iconv \
	--without-ident --without-tcp-wrappers --without-pam \
	--with-syslog --with-epoll --enable-ipv6
make
make DESTDIR=$PKG install

rm -f "$PKG/etc/ngircd.conf"

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/79_ngircd.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="ngircd"
DAEMON="/usr/sbin/ngircd"
CONF="/etc/ngircd.conf"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if [ ! -s "$CONF" ]; then
            echo "[SKIP] $NAME: no $CONF (/usr/share/doc/ngircd/sample-ngircd.conf is the template)"
            exit 0
        fi
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^ngircd:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no ngircd account (the port's postinstall makes it)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" setpriv --reuid=ngircd --regid=ngircd \
            --init-groups "$DAEMON" -n -f "$CONF"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/79_ngircd.sh"
