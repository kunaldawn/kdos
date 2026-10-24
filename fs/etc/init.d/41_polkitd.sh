#!/bin/bash
. /etc/init.d/service_helper

# STARTED HERE AND NEVER BY D-BUS ACTIVATION, and it has to be.
#
# Activation would run polkitd as root through `dbus-daemon-launch-helper`,
# and depending on one setuid bit for the whole of this machine's network
# authorisation is a dependency that fails silently: polkitd never starts,
# every NetworkManager check is refused, and the only symptom is a wifi toggle
# that does nothing. So the polkit port ships no activation file: with one,
# a caller that came before polkitd had its name would make the bus start a
# second, unsupervised polkitd, and whichever lost the name would exit — the
# supervised one then restarting every five seconds for as long as the other
# held it.
#
# AFTER dbus (40) AND BEFORE ModemManager and NetworkManager (42), which ask
# polkit on their first privileged call and would be refused while the name
# had no owner; `wait_bus_name` holds the boot here until polkitd has it.
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
        if ! grep -q '^polkitd:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no polkitd account (the port's postinstall makes it)"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/polkit-1
        # polkitd never forks, so the supervisor watches the daemon itself.
        # What polkitd logs goes to syslog as authpriv, so /var/log/auth.log;
        # it logs no allow or deny decision at any --log-level, and a rule
        # that wants one written calls polkit.log(), which prints to stderr.
        # Its stdout carries only a copy of each line it sends to syslog
        # and start-up chatter, so redirfd drops it: forwarded, every line
        # would be logged twice. Its stderr — polkit.log() and its error
        # messages — the supervisor forwards under authpriv, so it reaches
        # auth.log once and /var/log/messages not at all. --no-debug would
        # point stderr at /dev/null and discard polkit.log() with it.
        supervise --facility authpriv "$NAME" \
            /usr/bin/redirfd -w 1 /dev/null "$DAEMON"
        wait_bus_name org.freedesktop.PolicyKit1
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
