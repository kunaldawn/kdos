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

meson setup build \
	--prefix=/usr \
	--buildtype=release \
	-Dsystemdsystemunitdir=/usr/lib/systemd/system \
	-Dhwdbdir=/usr/lib/udev/hwdb.d \
	-Drulesdir=/usr/lib/udev/rules.d \
	-Dlibdrm=enabled \
	-Dlibdrm_amdgpu=enabled \
	-Dlibdrm_nouveau=enabled \
	-Ddrm_xe=enabled \
	-Dgtk_doc=false \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
rm -rf "$PKG/usr/lib/systemd"
install -d "$PKG/etc/init.d"
cat > "$PKG/etc/init.d/44_switcheroo-control.sh" <<'KDOS_SH'
#!/bin/bash
. /etc/init.d/service_helper

NAME="switcheroo-control"
DAEMON="/usr/libexec/switcheroo-control"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        gpus=0
        for c in /sys/class/drm/card[0-9]*; do
            case "${c##*/}" in *-*) continue ;; esac
            [ -e "$c/device" ] && gpus=$((gpus + 1))
        done
        if [ "$gpus" -lt 2 ]; then
            echo "[SKIP] $NAME: fewer than two GPUs"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
KDOS_SH
chmod 755 "$PKG/etc/init.d/44_switcheroo-control.sh"
