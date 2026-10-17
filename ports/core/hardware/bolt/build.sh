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

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--localstatedir=/var \
	--buildtype=release \
	-Dman=true \
	-Dinstall-tests=false \
	-Dprivileged-group=wheel
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

install -d "$PKG/usr/lib/udev/rules.d"
cat > "$PKG/usr/lib/udev/rules.d/91-kdos-bolt.rules" <<'RULES'
ACTION=="add", SUBSYSTEM=="thunderbolt", RUN+="/usr/bin/dbus-send --system --type=method_call --dest=org.freedesktop.bolt /org/freedesktop/bolt org.freedesktop.DBus.Peer.Ping"
RULES
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/43_boltd.sh" <<'KDOS_SH'
#!/bin/bash
NAME="boltd"

case "$1" in
    start)
        # No controller, nothing to authorise; the udev rule starts boltd if
        # one appears later.
        if [ -z "$(ls -A /sys/bus/thunderbolt/devices 2>/dev/null)" ]; then
            echo "[SKIP] $NAME: no thunderbolt controller"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        dbus-send --system --type=method_call \
            --dest=org.freedesktop.bolt /org/freedesktop/bolt \
            org.freedesktop.DBus.Peer.Ping
        ;;
    stop)
        # Started by the bus, so not ours to stop; it ends with the machine.
        ;;
    status)
        # Running is owning the bus name, which is also what a client needs.
        if dbus-send --system --print-reply --dest=org.freedesktop.DBus \
            / org.freedesktop.DBus.NameHasOwner string:org.freedesktop.bolt \
            2>/dev/null | grep -q 'boolean true'; then
            echo "$NAME is running"
        else
            echo "$NAME is not running"
            exit 1
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/43_boltd.sh"
