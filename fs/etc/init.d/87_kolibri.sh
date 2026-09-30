#!/bin/bash
. /etc/init.d/service_helper

# Kolibri, the offline curriculum, served to this machine alone at
# http://127.0.0.1:8081. 8080 is kiwix-serve's. Its database, its logs and the
# channels it serves live in KOLIBRI_HOME, /var/lib/kolibri, which the kolibri
# account owns; a channel pack installs its content there.
#
# Skipped until a channel is present: with no channel database under
# content/databases Kolibri has nothing to teach, and a Django server held
# resident for an empty catalogue is memory spent on nothing. The command
# /usr/bin/kolibri turns the statistics pingback off.

NAME="kolibri"
DAEMON="/usr/bin/kolibri"
HOME_DIR="/var/lib/kolibri"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        # Read /etc/passwd directly: there is no getent on musl.
        if ! grep -q '^kolibri:' /etc/passwd 2>/dev/null; then
            echo "[SKIP] $NAME: no kolibri account (the port's postinstall makes it)"
            exit 0
        fi
        if ! ls "$HOME_DIR"/content/databases/*.sqlite3 >/dev/null 2>&1; then
            echo "[SKIP] $NAME: no channel in $HOME_DIR/content/databases"
            exit 0
        fi
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" env HOME="$HOME_DIR" KOLIBRI_HOME="$HOME_DIR" \
            KOLIBRI_LISTEN_ADDRESS=127.0.0.1 \
            setpriv --reuid=kolibri --regid=kolibri --init-groups \
            "$DAEMON" start --foreground --port=8081
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
