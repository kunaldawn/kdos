#!/bin/bash
. /etc/init.d/service_helper

# STARTED HERE RATHER THAN LEFT TO D-BUS ACTIVATION, and it has to be.
#
# `org.freedesktop.PolicyKit1.service` asks dbus-daemon to run polkitd as
# `User=root`, and dbus-daemon runs as `messagebus` — it can only do that
# through `dbus-daemon-launch-helper`, which is setuid root on a distribution
# that relies on activation. Depending on one setuid bit for the whole of this
# machine's network authorisation is a dependency that fails silently: polkitd
# never starts, every NetworkManager check is refused, and the only symptom is
# a wifi toggle that does nothing.
#
# AFTER dbus (40) AND BEFORE NetworkManager (42). polkitd needs the system bus
# to own its name, and NetworkManager asks polkit on its first privileged call
# — a NetworkManager that came up first would simply be refused until polkitd
# arrived, which is a race nobody would attribute correctly.
#
# WHAT IT AUTHORISES IS /etc/polkit-1/rules.d/50-kdos.rules and nothing else of
# ours. There is no authentication agent on this system and there is not meant
# to be: see docs/kdos/03-architecture/security-model.md for why an agent
# cannot work here at all.

NAME="polkitd"
DAEMON="/usr/lib/polkit-1/polkitd"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # polkitd drops to this account after it binds; without it the daemon
        # exits at once and `supervise` would respawn it for ever.
        if ! getent passwd polkitd >/dev/null 2>&1; then
            echo "[SKIP] $NAME: no polkitd account (the port's postinstall makes it)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/polkit-1
        # --no-debug is the foreground mode; the supervisor owns the lifetime.
        supervise "$NAME" "$DAEMON" --no-debug
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
