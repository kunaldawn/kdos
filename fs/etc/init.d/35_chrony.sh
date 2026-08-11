#!/bin/bash
. /etc/init.d/service_helper

NAME="chronyd"
DAEMON="/usr/sbin/chronyd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/chrony /var/lib/chrony
        # After 30_network: chronyd started before an interface exists just
        # logs unreachable pools until the first poll interval expires.
        #
        # No hwclock call anywhere in KDOS. The kernel does both directions
        # itself — CONFIG_RTC_HCTOSYS sets the clock at boot, CONFIG_RTC_SYSTOHC
        # writes it back every 11 minutes — and chrony.conf's `rtcsync` tells
        # chronyd to leave the RTC to the kernel rather than fight it.
        #
        # -d = foreground (chronyd's own flag; it is not a debug switch).
        supervise "$NAME" "$DAEMON" -d
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
