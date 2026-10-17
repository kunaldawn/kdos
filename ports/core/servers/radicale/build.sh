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

_home=/usr/lib/radicale

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--ignore-installed --root=$PKG --prefix=$_home \
	defusedxml libpass vobject .

install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/radicale" <<'KDOS_SH'
#!/bin/sh
home=/usr/lib/radicale
site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$home") || exit 1
PYTHONPATH=$site${PYTHONPATH:+:$PYTHONPATH}
export PYTHONPATH
exec "$home/bin/radicale" "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/radicale"

install -Dm644 config "$PKG/usr/share/doc/radicale/config.example"
install -Dm644 rights "$PKG/usr/share/doc/radicale/rights.example"
install -Dm644 /dev/stdin "$PKG/etc/radicale/config" <<'CFG'
[server]
hosts = 0.0.0.0:5232, [::]:5232

[auth]
type = htpasswd
htpasswd_filename = /etc/radicale/users
htpasswd_encryption = autodetect

[rights]
type = owner_only

[storage]
filesystem_folder = /var/lib/radicale/collections
CFG
install -dm750 "$PKG/var/lib/radicale"

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/77_radicale.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="radicale"
DAEMON="/usr/bin/radicale"
USERS="/etc/radicale/users"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^radicale:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no radicale account (the port's postinstall makes it)"
            exit 0
        fi
        if ! grep -q '^[^#].*:' "$USERS" 2>/dev/null; then
            echo "[SKIP] $NAME: no account in $USERS (name:\$(openssl passwd -6))"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" setpriv --reuid=radicale --regid=radicale \
            --init-groups "$DAEMON" --config /etc/radicale/config
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/77_radicale.sh"
