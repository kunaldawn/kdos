#!/bin/bash
. /etc/init.d/service_helper

# org.freedesktop.hostname1, timedate1 and locale1: the machine's name, clock,
# time zone, locale and keyboards, for the Date & Time page, plasma-setup and
# the login screen, whose compositor takes its keyboard layout from locale1.
#
# AFTER polkitd (41), because every method asks it, and BEFORE the display
# manager, which inittab starts once rcS has finished: a login screen that
# came up before locale1 had an owner would read no layout and offer US
# QWERTY at the password prompt. `wait_bus_name` holds the boot here until
# all three names are owned, where there is a system bus to own them on;
# one name not taken in ten seconds is the daemon failing, and the boot goes
# on rather than waiting ten more for each of the other two. There is no
# activation file, so a call made before then is refused rather than
# starting a second, unsupervised copy.
#
# --restart-with dbus: the daemon exits when its bus connection goes, and
# comes back with the bus rather than five seconds after it. Exit 3 is
# another process owning one of the names, which a restart cannot change.

NAME="kdos-settingsd"
DAEMON="/usr/sbin/kdos-settingsd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise --restart-with dbus --final-exit 3 "$NAME" "$DAEMON"
        [ -S /run/dbus/system_bus_socket ] || exit 0
        wait_bus_name org.freedesktop.hostname1 &&
            wait_bus_name org.freedesktop.timedate1 &&
            wait_bus_name org.freedesktop.locale1
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
