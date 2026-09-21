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
        # INTEL ONLY, AND IT SAYS SO RATHER THAN RESPAWNING. thermald refuses
        # to start on a machine with no Intel thermal zones, and `supervise`
        # would restart a refusing daemon for ever — so the check is here,
        # before the respawn loop exists. The same rule 57_oomd.sh keeps
        # about PSI.
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
        supervise "$NAME" "$DAEMON" --no-daemon --adaptive
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
