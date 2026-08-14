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
        "$DAEMON" --daemon
        # --action=add, and not the default. udevadm trigger replays every
        # device with action "change", but /lib/udev/rules.d/80-drivers.rules
        # — the rule that modprobes a driver from MODALIAS — opens with
        # ACTION!="add", GOTO="drivers_end". A plain trigger therefore loads
        # NO module at all: the HDA controller stayed unclaimed, "No
        # soundcards found", alsa-lib answered "Unknown PCM default" and the demo
        # aborted inside MikMod_Init. Subsystems before devices, so a bus
        # module is in place before its children are replayed.
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
