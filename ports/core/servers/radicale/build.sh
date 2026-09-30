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


# THE CLOSURE LIVES UNDER ITS OWN PREFIX, /usr/lib/radicale. vobject is
# also vendored into site-packages by khard, and two packages owning one path
# is a conflict that stops the install; under the prefix nothing else can own
# defusedxml, libpass or vobject. requests, packaging, dateutil, pytz and six
# are ports, found in site-packages through depends.
#
# vobject IS PINNED BELOW ITS LATEST. 0.9.9 reads its version by importing
# the package, which imports dateutil, and the isolated environment a
# source-only download builds metadata in has no dateutil: the fetch fails.
# 0.9.8 declares it statically, and Radicale asks for 0.9.6 or later.
#
# pika, which Radicale declares, is imported only by the RabbitMQ hook, a
# plugin loaded by name from the configuration; there is no broker for it on
# this system and it is not installed.
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

# THE SHIPPED CONFIGURATION SERVES THE LAN, WITH ACCOUNTS. Upstream's default
# listens on localhost and denies everyone, which serves nobody; this listens
# on every address and takes accounts from /etc/radicale/users, one
# `name:hash` line each, a hash `openssl passwd -6` writes. owner_only rights
# give each account its own calendars and address books. The port is still
# closed to other machines until the firewall's rule for it is on.
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

# THE SERVER RUNS UNDER ksvc, AS ITS ACCOUNT, and it skips until an account
# exists: with no line in the users file there is nobody it can let in, and
# every machine carrying the port would otherwise hold 5232 open for nothing.
# Radicale stays in the foreground and logs to stderr, which the supervisor
# keeps.
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
