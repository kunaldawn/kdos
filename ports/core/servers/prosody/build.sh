# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

./configure --prefix=/usr --sysconfdir=/etc/prosody \
	--datadir=/var/lib/prosody \
	--lua-version=$_lv \
	--with-lua-bin=/usr/bin \
	--with-lua-include=/usr/include/lua$_lv \
	--with-lua-lib=/usr/lib \
	--lua-suffix=$_lv \
	--idn-library=idn \
	--with-random=getrandom \
	--no-example-certs \
	--add-cflags="$CFLAGS" \
	--add-ldflags="$LDFLAGS"
make
make DESTDIR=$PKG install

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/74_prosody.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="prosody"
DAEMON="/usr/bin/prosody"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^prosody:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no prosody account (the port's postinstall makes it)"
            exit 0
        fi
        # Accounts live at <data>/<host>/accounts/<user>.dat; the first is
        # made with `prosodyctl adduser <user>@<host>`.
        if ! compgen -G '/var/lib/prosody/*/accounts/*.dat' >/dev/null; then
            echo "[SKIP] $NAME: no account yet (prosodyctl adduser makes one)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" setpriv --reuid=prosody --regid=prosody \
            --init-groups "$DAEMON" -F
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/74_prosody.sh"
