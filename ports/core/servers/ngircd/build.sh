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


# TLS through OpenSSL and link compression through zlib, both named so
# neither depends on what configure finds. IDENT lookups, TCP wrappers and
# PAM are off: none of their libraries is a port, and a LAN server has no
# ident daemon to ask. iconv is musl's own, for the CHARCONV channel mode.
./configure --prefix=/usr --sysconfdir=/etc \
	--with-openssl --without-gnutls \
	--with-zlib --with-iconv \
	--without-ident --without-tcp-wrappers --without-pam \
	--with-syslog --with-epoll --enable-ipv6
make
make DESTDIR=$PKG install

# THE SAMPLE IS NOT THE CONFIGURATION. The install writes a copy of it to
# /etc/ngircd.conf, and a file there is what starts the service below, so it
# is removed; the sample stays in /usr/share/doc/ngircd, and copying it into
# place is the decision to run a server.
rm -f "$PKG/etc/ngircd.conf"

# THE SERVER RUNS UNDER ksvc ONCE /etc/ngircd.conf EXISTS, as the
# postinstall's account: -n keeps it in the foreground where the supervisor
# watches it, and setpriv drops root before the exec, so ServerUID in the
# configuration is not needed. Ports 6667 and 6697 need no privilege.
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
