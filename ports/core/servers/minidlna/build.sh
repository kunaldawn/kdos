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

patch -p1 -i "$PORT_SRC/ffmpeg7.patch"
patch -p1 -i "$PORT_SRC/libav-fix.patch"
autoreconf -fi

export CFLAGS="$CFLAGS -fcommon"
./configure --prefix=/usr --sysconfdir=/etc \
	ac_cv_lib_avahi_client_avahi_threaded_poll_new=no \
	--with-db-path=/var/lib/minidlna \
	--with-log-path=/var/log/minidlna \
	--with-os-name=KDOS --with-os-url=https://github.com/kunaldawn/kdos \
	--disable-nls --disable-tivo --disable-netgear --disable-readynas
make
make DESTDIR=$PKG install

install -Dm644 minidlna.conf.5 "$PKG/usr/share/man/man5/minidlna.conf.5"
install -Dm644 minidlnad.8 "$PKG/usr/share/man/man8/minidlnad.8"
install -Dm644 minidlna.conf "$PKG/usr/share/doc/minidlna/minidlna.conf.example"
install -dm755 "$PKG/var/lib/minidlna" "$PKG/var/log/minidlna"

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/84_minidlna.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="minidlna"
DAEMON="/usr/sbin/minidlnad"
CONF="/etc/minidlna.conf"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if [ ! -s "$CONF" ]; then
            echo "[SKIP] $NAME: no $CONF (/usr/share/doc/minidlna has the template)"
            exit 0
        fi
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^minidlna:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no minidlna account (the port's postinstall makes it)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON" -S -u minidlna -f "$CONF"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/84_minidlna.sh"
