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

./configure --prefix=/usr --cfgdir=/etc --disable-x11 --disable-debug
make
make DESTDIR=$PKG install
rm -f "$PKG/usr/bin/spnavd_ctl"
install -Dm644 doc/example-spnavrc "$PKG/usr/share/doc/spacenavd/example-spnavrc"
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/61_spacenavd.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="spacenavd"
DAEMON="/usr/bin/spacenavd"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON" -d -l syslog
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/61_spacenavd.sh"
