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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

export CGO_ENABLED=1
go build -mod=vendor -ldflags "-s -w -X main.Version=$version" -o step-ca ./cmd/step-ca
install -Dm755 step-ca $PKG/usr/bin/step-ca

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/89_step-ca.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="step-ca"
DAEMON="/usr/bin/step-ca"
export STEPPATH="/etc/step-ca"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if [ ! -s "$STEPPATH/config/ca.json" ]; then
            echo "[SKIP] $NAME: no $STEPPATH/config/ca.json (STEPPATH=$STEPPATH step ca init makes it)"
            exit 0
        fi
        if [ ! -s "$STEPPATH/password.txt" ]; then
            echo "[SKIP] $NAME: no $STEPPATH/password.txt"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON" "$STEPPATH/config/ca.json" \
            --password-file "$STEPPATH/password.txt"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/89_step-ca.sh"
