#!/bin/bash
# THE "INSTALL KDOS" ENTRY, ON THE LIVE MEDIUM AND NOWHERE ELSE.
#
# kdos-installer-gui ships its desktop entry and its polkit rule under
# /usr/share/kdos/installer, where nothing reads them. On the live medium —
# the root is the overlay the initramfs built, which no installed system's
# root is — this step puts them where they are read:
#
#   /usr/share/applications/kdos-installer.desktop   the launcher entry, which
#       the welcome tour's live page also opens (LiveInstaller= in
#       /usr/share/kdos/xdg-live/plasma-welcomerc names it)
#   ~kdos/Desktop/kdos-installer.desktop             an icon on the desktop,
#       executable, which is what makes Plasma trust a launcher in a home
#   /etc/polkit-1/rules.d/49-kdos-installer-live.rules  the installer and
#       Partition Manager with no password, for the live session at the
#       machine
#
# All three are written into the overlay, so they last one boot, and kinstall
# leaves them out of the copy: an installed system never offers to install
# itself over its own disk.
#
# BEFORE SDDM, so the session's menu cache is built with the entry in it.
# polkitd (41) watches its rules directory, so a rule added after it started
# is read.

NAME="live-installer"
SRC=/usr/share/kdos/installer
ENTRY=kdos-installer.desktop
RULE=49-kdos-installer-live.rules
LIVE_USER=kdos

live() {
    while read -r _dev _mnt _type _rest; do
        if [ "$_mnt" = / ] && [ "$_type" = overlay ]; then
            return 0
        fi
    done < /proc/mounts
    return 1
}

case "$1" in
    start)
        if ! live; then
            exit 0
        fi
        if [ ! -f "$SRC/$ENTRY" ]; then
            echo "[SKIP] $NAME: the graphical installer is not on this medium; run kinstall"
            exit 0
        fi
        echo "[KDOS] Offering the installer..."
        install -D -m 0644 "$SRC/$ENTRY" "/usr/share/applications/$ENTRY"
        [ -f "$SRC/$RULE" ] && install -D -m 0644 "$SRC/$RULE" "/etc/polkit-1/rules.d/$RULE"
        _pw=$(grep "^$LIVE_USER:" /etc/passwd)
        if [ -n "$_pw" ]; then
            IFS=: read -r _n _x _uid _gid _gecos _home _shell <<< "$_pw"
            if [ -d "$_home" ]; then
                install -d -m 0755 -o "$_uid" -g "$_gid" "$_home/Desktop"
                install -m 0755 -o "$_uid" -g "$_gid" "$SRC/$ENTRY" "$_home/Desktop/$ENTRY"
            fi
        fi
        ;;
    stop)
        # One-shot — nothing to stop
        ;;
    status)
        if [ -f "/usr/share/applications/$ENTRY" ]; then
            echo "[ OK ] $NAME: offered"
        else
            echo "[ OK ] $NAME: not offered (not the live medium)"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
