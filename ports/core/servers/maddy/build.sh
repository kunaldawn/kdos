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
go build -mod=vendor -trimpath -tags libsqlite3 \
	-ldflags "-s -w -X github.com/foxcpp/maddy.Version=$version" \
	-o build/maddy ./cmd/maddy
install -Dm755 build/maddy "$PKG/usr/bin/maddy"

scdoc < docs/man/maddy.1.scd > build/maddy.1
install -Dm644 build/maddy.1 "$PKG/usr/share/man/man1/maddy.1"

install -Dm644 maddy.conf "$PKG/etc/maddy/maddy.conf"
install -dm750 "$PKG/var/lib/maddy"

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/78_maddy.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="maddy"
DAEMON="/usr/bin/maddy"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^maddy:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no maddy account (the port's postinstall makes it)"
            exit 0
        fi
        if [ ! -s /var/lib/maddy/credentials.db ]; then
            echo "[SKIP] $NAME: no account yet (run maddy creds create and maddy imap-acct create as the maddy account)"
            exit 0
        fi
        install -d -o maddy -g maddy -m 750 /run/maddy
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" setpriv --reuid=maddy --regid=maddy --init-groups \
            --inh-caps=+net_bind_service --ambient-caps=+net_bind_service \
            "$DAEMON" --config /etc/maddy/maddy.conf run
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/78_maddy.sh"
