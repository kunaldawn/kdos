#!/bin/bash
. /etc/init.d/service_helper

# org.freedesktop.sysupdate1 and io.kdos.Slots1: the system update Discover
# shows, the slots the System Versions page lists, and the root half of the
# session-confirmed boot — kdos-sysupdated runs `kdos-bootctl mark-good`
# once rcS's mark and the session's (or the greeter's) are both under
# /run/kdos/bless. Disabled, nothing reads the marks, and rcS confirms the
# boot itself as soon as its services have started.
#
# AFTER polkitd (41), which every changing method asks, and elogind (40),
# which polkit asks in turn whether a caller is the active local session.
# `wait_bus_name` holds the boot until the name is owned. There is no
# activation file, so a call made before then is refused rather than
# starting a second, unsupervised copy that would keep this one from its
# names.
#
# --restart-with dbus: the daemon exits when its bus connection goes and
# comes back with the bus. Exit 3 is another process owning one of the
# names, which a restart cannot change.

NAME="kdos-sysupdated"
DAEMON="/usr/sbin/kdos-sysupdated"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise --restart-with dbus --final-exit 3 "$NAME" "$DAEMON"
        wait_bus_name org.freedesktop.sysupdate1
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
