#!/bin/bash
. /etc/init.d/service_helper

# org.freedesktop.Accounts, FROM accounts-daemon, SUPERVISED, AND NEVER STARTED
# BY THE BUS.
#
# The human accounts as the desktop sees them: System Settings' Users page
# lists, creates and changes them through it, plasma-setup makes the first
# account through it, the portal's account dialog reads the name and picture
# from it, and the login screen shows the avatar it keeps in
# /var/lib/AccountsService/icons. It asks polkit before every change, so it
# comes AFTER polkitd (41); and it comes before the display manager, which
# inittab starts once rcS has finished, so the greeter's first look at the
# accounts finds the name owned.
#
# `wait_bus_name` holds the boot here until the name is owned. The package
# ships no activation file, so a call made before then is refused rather than
# starting a second, unsupervised daemon that would then hold the name the
# supervised one needs.
#
# --restart-with dbus: a restarted system bus leaves the daemon holding a name
# on a bus that is gone. It makes /var/lib/AccountsService/users (0700) and
# icons (0775) itself as it starts, which the package cannot carry.

NAME="accounts-daemon"
DAEMON="/usr/lib/accounts-daemon"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise --restart-with dbus "$NAME" "$DAEMON"
        [ -S /run/dbus/system_bus_socket ] || exit 0
        wait_bus_name org.freedesktop.Accounts
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
