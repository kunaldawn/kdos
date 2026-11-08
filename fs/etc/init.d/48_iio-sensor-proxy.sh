#!/bin/bash
. /etc/init.d/service_helper

NAME="iio-sensor-proxy"
DAEMON="/usr/libexec/iio-sensor-proxy"

case "$1" in
    start)
        if [ ! -x "$DAEMON" ]; then
            echo "[SKIP] $NAME: $DAEMON not found"
            exit 0
        fi
        # STARTED ONLY WHERE THERE IS A SENSOR. The daemon's own udev rule
        # tags every accelerometer, light sensor, proximity sensor and compass
        # it can drive with IIO_SENSOR_PROXY_TYPE, and a daemon that finds
        # none exits 0 at once, which `supervise` would restart every five
        # seconds for as long as the machine is up. The tag in udev's
        # database, settled by 01_udev.sh, is the daemon's own test made
        # before the respawn loop exists. A sensor that appears after boot is
        # started with `service start iio-sensor-proxy`. Built-in sensors do
        # not leave; a removable one that does leaves the daemon exiting 0
        # every five seconds until `service stop iio-sensor-proxy`, and
        # --final-exit 0 cannot end that, because losing its bus name is an
        # exit 0 too and that one has to be restarted.
        if ! udevadm info --export-db 2>/dev/null |
                grep -q '^E: IIO_SENSOR_PROXY_TYPE='; then
            echo "[SKIP] $NAME: no sensor its udev rule recognises"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        # net.hadess.SensorProxy, for KWin's screen rotation and PowerDevil's
        # ambient-light brightness; each claims a sensor through polkit's
        # net.hadess.SensorProxy.claim-sensor, which the active session has.
        # Supervised and never activated, for service_helper's reason. The
        # boot does not wait for the name: its clients watch for it, and a
        # sensor is not something the rest of the boot needs.
        #
        # --restart-with dbus: the daemon exits when it loses its name, which
        # is what a bus restart does to it.
        supervise --restart-with dbus "$NAME" "$DAEMON"
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
