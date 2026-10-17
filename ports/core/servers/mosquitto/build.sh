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

mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DWITH_WEBSOCKETS=ON -DWITH_WEBSOCKETS_BUILTIN=ON \
	-DWITH_DOCS=ON -DWITH_TESTS=OFF \
	-DWITH_TLS=ON \
	-DWITH_HTTP_API=ON -DHTTP_API_DIR= \
	-DWITH_PLUGIN_PERSIST_SQLITE=ON \
	-DWITH_CTRL_SHELL=ON \
	-DWITH_PLUGIN_EXAMPLES=OFF \
	-DWITH_SYSTEMD=OFF -DWITH_SRV=OFF -DWITH_DLT=OFF
make
make DESTDIR=$PKG install

install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/73_mosquitto.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="mosquitto"
DAEMON="/usr/sbin/mosquitto"
CONF="/etc/mosquitto/mosquitto.conf"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        if [ ! -s "$CONF" ]; then
            echo "[SKIP] $NAME: no $CONF"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON" -c "$CONF"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/73_mosquitto.sh"
