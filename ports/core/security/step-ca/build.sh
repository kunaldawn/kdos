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

# WHAT THIS SOLVES IS THE BROWSER WARNING, and on an island network there is no
# other answer. Every service worth running here — kiwix, a git forge, a
# printer's admin page — wants https, and the two usual routes are both closed:
# Let's Encrypt needs the internet, and a self-signed certificate per host
# means a warning per host that people learn to click through. step-ca issues
# from ONE root that gets trusted once, and it speaks ACME, so caddy renews
# against it automatically with the same config it would use publicly.
#
# caddy's `tls internal` is the smaller answer for one machine; this is the one
# for a network with several. The server only runs a ca.json it is given: the
# `step` command (step-cli) creates the CA with `step ca init` and is the
# client for everything after.
#
# cgo is what builds the two hardware key stores, so the CA's root key can
# live off the disk. The PKCS#11 one dlopens whatever module the config names
# and links nothing; the YubiKey PIV one links libpcsclite through pkg-config
# and talks to the card through pcscd. With cgo off both compile out silently
# and a `kms` block naming either fails only when the CA starts.
export CGO_ENABLED=1
go build -mod=vendor -ldflags "-s -w -X main.Version=$version" -o step-ca ./cmd/step-ca
install -Dm755 step-ca $PKG/usr/bin/step-ca

# The CA runs from /etc/step-ca, as root, which keeps the root key and its
# password file off every user account. `sudo STEPPATH=/etc/step-ca step ca
# init` writes config/ca.json there; the password that init asks for goes in
# password.txt beside it, mode 600, because a supervised start has nobody to
# type it. The service is skipped until both exist.
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
