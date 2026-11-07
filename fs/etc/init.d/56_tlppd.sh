#!/bin/bash
. /etc/init.d/service_helper

NAME="tlp-pd"
DAEMON="/usr/sbin/tlp-pd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # Power profiles for PowerDevil and the battery applet, served as
        # org.freedesktop.UPower.PowerProfiles and net.hadess.PowerProfiles:
        # each switch runs `tlp power-saver|balanced|performance`, so TLP's
        # own settings stay the tuning under every profile. Numbered after
        # 55_tlp.sh, which applies the boot profile tlp-pd reads back: a
        # shared 55_ prefix would leave the order to the locale's collation,
        # which sorts 55_tlppd.sh first under any UTF-8 one.
        #
        # SUPERVISED, NEVER ACTIVATED. TLP's D-Bus service files name only a
        # systemd unit and Exec=/bin/false, so the port installs none, and a
        # caller that comes before the name is owned is refused rather than
        # handed a copy nobody supervises. wait_bus_name holds the boot until
        # the name is taken, so the session's PowerDevil finds it the first
        # time it looks.
        #
        # --restart-with dbus: tlp-pd's connection, and the process with it,
        # ends with the bus. Restarted with the bus it takes its names on the
        # new one as soon as that one answers.
        supervise --restart-with dbus "$NAME" "$DAEMON"
        [ -S /run/dbus/system_bus_socket ] && \
            wait_bus_name org.freedesktop.UPower.PowerProfiles
        exit 0
        ;;
    stop)
        stop_service "$NAME"
        ;;
    status)
        check_status "$NAME"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
