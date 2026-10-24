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

make CDEBUGFLAGS="$CFLAGS -Wall"
make PREFIX=/usr TARGET=$PKG install

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/31_babeld.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="babeld"
DAEMON="/usr/bin/babeld"
CONF="/etc/babeld.conf"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if [ ! -s "$CONF" ]; then
            echo "[SKIP] $NAME: no $CONF"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON" -I '' -c "$CONF" -S /var/lib/babeld/state
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/31_babeld.sh"
install -dm755 "$PKG/var/lib/babeld"
