#!/bin/bash
. /etc/init.d/service_helper

NAME="cupsd"
DAEMON="/usr/sbin/cupsd"

# cups-browsed and ipp-usb exist only to hand printers to cupsd, so they start
# and stop with it: whatever starts cups (rcS, `service start cups`) brings them
# up without a reboot, and stopping cups takes them down first. Each still has
# its own marker; a companion disabled on its own is not started here. rcS's own
# later start of 81 and 82 finds them supervised and prints `[SKIP] already
# supervised`.
COMPANIONS=("81_cups-browsed:cups-browsed" "82_ipp-usb:ipp-usb")

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/cups
        # -f = run in foreground for supervision
        supervise "$NAME" "$DAEMON" -f
        _rc=$?
        if [ "$_rc" = 0 ]; then
            for _c in "${COMPANIONS[@]}"; do
                [ -x "/etc/init.d/${_c%%:*}.sh" ] || continue
                [ -f "/etc/service.disabled/${_c#*:}" ] && continue
                "/etc/init.d/${_c%%:*}.sh" start
            done
        fi
        exit "$_rc"
        ;;
    stop)
        # The companions first: neither has anything to do once cupsd is gone.
        # A companion that is not running answers "not running", and cupsd is
        # still stopped.
        for ((_i = ${#COMPANIONS[@]} - 1; _i >= 0; _i--)); do
            _c=${COMPANIONS[_i]}
            [ -x "/etc/init.d/${_c%%:*}.sh" ] || continue
            "/etc/init.d/${_c%%:*}.sh" stop || true
        done
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
