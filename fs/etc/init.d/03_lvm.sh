#!/bin/bash
. /etc/init.d/service_helper

NAME="lvm"
LVM="/usr/sbin/lvm"

# Activates the LVM volume groups of the running system. lvm2 is built with
# event activation off and without its hotplug rule, 69-dm-lvm.rules, which
# activates a group through systemd-run; so nothing on hotplug activates a
# group, and a disk carrying LVM shows its logical volumes only once this has
# run — at boot, or by hand with `vgchange -aay`. The installer runs the same
# activation before it lists volumes.
#
# A disk boot has already activated every group the initramfs could see,
# because a root on a logical volume has to exist before switch_root. Those
# groups are active here and vgchange leaves them as they are; this pass is
# what activates the groups of a live boot, which the initramfs does not
# touch, and those on disks that appeared after it ran. It still reports
# every active group as "now active", so the fsck and `mount -a` below run on
# every boot that has a group; both leave what is already mounted alone.
#
# AFTER udev, which has to be running for lvm's udev synchronisation and to make
# the /dev/<vg>/<lv> links, and which therefore comes after rcS's `mount -a`.
# An fstab entry on a group the initramfs did not activate has no device during
# rcS's fsck and `mount -a`, and is checked and mounted here instead: a second
# `fsck -A -R -M` checks each entry with a pass number that is not mounted, then
# a second `mount -a` mounts it. -M is what leaves the filesystems rcS already
# mounted unchecked; a check of a mounted filesystem is a check that can corrupt
# it. util-linux's mount resolves UUID= and LABEL= tags and skips every entry
# that is already mounted, so the entries rcS mounted are left as they are. The
# report is appended to rcS's, /run/kdos-fsck.log.

case "$1" in
    start)
        if [ ! -x "$LVM" ]; then
            echo "[SKIP] $NAME: $LVM not found"
            exit 0
        fi
        _out=$("$LVM" vgchange -aay --sysinit 2>&1)
        [ -n "$_out" ] && echo "$_out"
        case "$_out" in
            *"now active"*) ;;
            *) exit 0 ;;
        esac
        if command -v fsck >/dev/null 2>&1; then
            fsck -A -R -M -T -a </dev/null >>/run/kdos-fsck.log 2>&1
            _rc=$?
            if [ $((_rc & 4)) -ne 0 ]; then
                echo "[WARN] $NAME: fsck left errors uncorrected, see /run/kdos-fsck.log"
            elif [ $((_rc & 8)) -ne 0 ]; then
                echo "[WARN] $NAME: fsck could not check a filesystem, see /run/kdos-fsck.log"
            fi
        fi
        mount -a || echo "[WARN] $NAME: mount -a reported a failure"
        swapon -a 2>/dev/null || true
        ;;
    stop)
        # Nothing: the volumes are still mounted when rcK runs, and the
        # kernel lets them go at power-off.
        ;;
    status)
        "$LVM" lvs 2>/dev/null
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
