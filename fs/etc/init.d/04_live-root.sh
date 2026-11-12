#!/bin/bash
# THE LIVE ROOT IS NOT A DEVICE, so it is kept out of the desktop's Places.
#
# On the live medium / is the overlay the initramfs built over the squashfs
# and a RAM or persistence upper. Solid lists an overlay mounted on / as a
# storage device, and Dolphin, every file dialog and the Device Notifier then
# show a nameless entry with a capacity bar. Solid hides any mount whose
# options carry x-gvfs-hide, and it reads the options through libmount, which
# adds the userspace options recorded in /run/mount/utab to the kernel's.
# This step records x-gvfs-hide for / there.
#
# utab, not /etc/fstab: kinstall copies the live /etc/fstab and the live root
# into the installed system, and /run is left out of that copy, so the entry
# lasts one boot and never reaches an installed machine. An installed root is
# never an overlay, so there this step does nothing.

NAME="live-root"
UTAB=/run/mount/utab
ENTRY="SRC=overlay TARGET=/ ROOT=/ OPTS=x-gvfs-hide"

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
        live || exit 0
        mkdir -p "${UTAB%/*}"
        if ! grep -qxF "$ENTRY" "$UTAB" 2>/dev/null; then
            echo "$ENTRY" >> "$UTAB"
        fi
        echo "[KDOS] $NAME: hidden from Places"
        ;;
    stop|status)
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
