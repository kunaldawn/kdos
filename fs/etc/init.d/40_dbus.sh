#!/bin/bash
. /etc/init.d/service_helper

NAME="dbus"
DAEMON="/usr/bin/dbus-daemon"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        mkdir -p /run/dbus /var/lib/dbus
        # GetMachineId fails without this file and the bus never creates it
        # itself; the id must differ per machine, so it is made on first boot
        # and never carried in an image.
        [ -s /var/lib/dbus/machine-id ] || dbus-uuidgen --ensure
        # /etc/machine-id is where machine-id(5) puts the same id, and a
        # program written against that page reads it and nothing else — a
        # box that binds the host's id in, an Electron or Chromium build —
        # and gets no id at all without it. A link, so one
        # file holds the id and the installer's exclusion of that file is the
        # only one needed; -L keeps a link the installer copied, dangling
        # until the line above writes its target, from being made again.
        [ -e /etc/machine-id ] || [ -L /etc/machine-id ] || \
            ln -s /var/lib/dbus/machine-id /etc/machine-id
        # --nofork keeps it in foreground for supervision. --nopidfile,
        # because dbus-daemon leaves /run/dbus/pid behind when it exits on
        # SIGTERM or dies, and refuses to start while the file exists: the
        # supervisor's restart, and every `service restart dbus`, would fail
        # for as long as the machine ran. `supervise` starts nothing for a
        # name whose supervisor is alive, and that is the guard against a
        # second bus instead: it would take the socket from the first, and
        # every client already connected would be on a bus nobody else can
        # reach. --syslog-only, because dbus-daemon otherwise writes each
        # line to syslog and to stderr, which the supervisor logs again, and
        # every line reaches the log twice; the services the bus starts
        # still print to its stderr, /run/kdos-svc.dbus.log.
        supervise "$NAME" "$DAEMON" --system --nofork --nopidfile --syslog-only
        # The daemons after this connect at once, and one that finds no
        # socket yet exits and is started again five seconds later.
        wait_bus_name org.freedesktop.DBus
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
