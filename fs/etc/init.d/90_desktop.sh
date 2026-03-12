#!/bin/bash
. /etc/init.d/service_helper

start_one() {
    local name="$1"
    local daemon="$2"
    shift 2
    local args="$@"

    if [ ! -x "$daemon" ]; then
        echo "[SKIP] $name: $daemon not found"
        return
    fi
    echo "[KDOS] Starting $name..."
    supervise "$name" "$daemon $args"
}

case "$1" in
    start)
        # ConsoleKit2: Session tracker
        start_one "console-kit-daemon" "/usr/lib/consolekit2/console-kit-daemon" "--no-daemon"

        # Polkit: Authorization daemon
        start_one "polkitd" "/usr/lib/polkit-1/polkitd" "--no-debug"

        # UDisks2: Disk/storage management
        start_one "udisksd" "/usr/lib/udisks2/udisksd" "--no-debug"

        # UPower: Power management
        start_one "upowerd" "/usr/lib/upower/upowerd"
        ;;
    stop)
        stop_service "upowerd"
        stop_service "udisksd"
        stop_service "polkitd"
        stop_service "console-kit-daemon"
        ;;
    status)
        check_status "console-kit-daemon"
        check_status "polkitd"
        check_status "udisksd"
        check_status "upowerd"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
