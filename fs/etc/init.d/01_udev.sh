#!/bin/bash
. /etc/init.d/service_helper

NAME="udev"
DAEMON="/usr/sbin/udevd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -pv /run/udev
        chmod 755 /run/udev
        # The nodes of modules that load when their node is OPENED: uhid,
        # uinput, snd/seq, vhost-net, cuse and the rest of modules.devname.
        # No uevent names them before the module loads and nothing loads the
        # module until the node exists, so without this /dev/uhid never
        # appears and every Bluetooth keyboard and mouse pairs and then types
        # nothing. Made BEFORE udevd starts, because udevd applies the rules'
        # static_node= ownership (snd/seq to audio, vhost-net to kvm) to the
        # nodes it finds at start-up and to no others.
        if [ -x /usr/bin/kmod ]; then
            /usr/bin/kmod static-nodes --format=tmpfiles 2>/dev/null |
            while read -r _t _p _m _u _g _a _dev; do
                case "$_t" in c*|b*) ;; *) continue ;; esac
                [ -e "$_p" ] && continue
                mkdir -p "${_p%/*}"
                mknod -m "$_m" "$_p" "${_t%%!*}" "${_dev%%:*}" "${_dev##*:}"
            done
        fi
        "$DAEMON" --daemon
        # --action=add, and not the default. udevadm trigger replays every
        # device with action "change", but /lib/udev/rules.d/80-drivers.rules
        # — the rule that modprobes a driver from MODALIAS — opens with
        # ACTION!="add", GOTO="drivers_end". A plain trigger therefore loads
        # NO module at all: the sound controller stays unclaimed and ALSA
        # finds no card. Subsystems before devices, so a bus module is in
        # place before its children are replayed.
        /usr/sbin/udevadm trigger --action=add --type=subsystems
        /usr/sbin/udevadm trigger --action=add --type=devices
        /usr/sbin/udevadm settle
        echo "[KDOS] $NAME ready"
        ;;
    stop)
        echo "[KDOS] Stopping $NAME..."
        killall udevd 2>/dev/null
        ;;
    status)
        if pgrep -x udevd >/dev/null 2>&1; then
            echo "[ OK ] $NAME is running"
        else
            echo "[DOWN] $NAME"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
