#!/bin/bash
. /etc/init.d/service_helper

NAME="thermald"
DAEMON="/usr/sbin/thermald"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # INTEL ONLY, AND IT SAYS SO RATHER THAN RESPAWNING. A machine that
        # is not Intel is skipped here, before any supervisor exists. An
        # Intel model thermald does not know, with no thermal-conf.xml for
        # the platform — older parts, most virtual machines — passes this
        # check and is found out by thermald itself, which then exits 2.
        # `--final-exit 2` makes that exit the supervisor's last, where
        # without it ksvc would restart a refusing daemon every five seconds
        # for as long as the machine is up.
        if ! grep -qi 'GenuineIntel' /proc/cpuinfo 2>/dev/null; then
            echo "[SKIP] $NAME: not an Intel processor"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # --no-daemon: ksvc owns the process, so forking would hand the
        # supervisor a pid that exits immediately and be respawned for ever.
        # --adaptive uses the firmware's own DPTF tables where the machine
        # ships them, which is what makes it match the vendor's tuning
        # rather than override it.
        supervise --final-exit 2 "$NAME" "$DAEMON" --no-daemon --adaptive
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
