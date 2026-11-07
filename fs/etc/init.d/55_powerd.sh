#!/bin/bash
. /etc/init.d/service_helper

NAME="kdos-powerd"
DAEMON="/usr/sbin/kdos-powerd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # The firewall's named services and the accent of the boot menu, the
        # splash and the console are root's files, and the desktop is not
        # root. The daemon is what stands between them, with two faces: a
        # socket in /run gated by SO_PEERCRED (root and wheel; anyone else
        # gets polkit's answer, asked without a prompt) for the `kdos`
        # command, and io.kdos.Power1 on the system bus, asked of polkit per
        # call, for System Settings. Suspend, power-off and reboot are
        # elogind's, not this daemon's.
        #
        # It runs in the foreground by design — `supervise` owns the respawn —
        # and ships no D-Bus activation file: a caller that came before the
        # name was owned would make the bus start a second, unsupervised copy.
        # wait_bus_name holds the boot until the name is taken, where there is
        # a system bus to take it on. The daemon answers the socket whether or
        # not the bus is up and retries the bus itself, so a bus restart needs
        # no --restart-with.
        supervise "$NAME" "$DAEMON"
        [ -S /run/dbus/system_bus_socket ] && wait_bus_name io.kdos.Power1
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
