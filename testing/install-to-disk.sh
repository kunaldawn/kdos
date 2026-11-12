#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   Install KDOS onto the attached disk, without a human.
#
#   Runs IN THE GUEST, as root, on a booted live ISO with a virtio disk:
#     testing/vnc-shot.py --disk build/kdos-target.qcow2 --no-session \
#                         --root-script testing/install-to-disk.sh
#
# WHY. A live session keeps $HOME on overlayfs, where an overlay upper cannot
# sit, so its boxes run on a tmpfs layer under /run/user/<uid>/kdos/boxes that
# ends with the session (see known-gaps.md, "A live session cannot create a
# persistent box"). A box that persists, `kdos-box freeze`, telemetry naming a
# box across a reboot and the installer's own Applications page need a machine
# that was installed, and this is the cheapest way to get one.
#
# THE TARGET MUST HOLD THE PAYLOAD. The copy writes about as much as the
# `payload` line of `kinstall --dump probe` into the disk, so a qcow2 grows
# on the host by that much. kinstall's Prepare step refuses a disk smaller
# than the payload and 768 MiB. This script's gate runs before the installer
# starts, sets a higher bar (the payload, the 512 MB ESP and 2 GiB), and prints
# both numbers, so a refused run says how far short the disk is. vnc-shot.py
# refuses an install run whose qcow2 could outgrow the host's free space.
#
# THE ANSWER FILE IS WRITTEN HERE RATHER THAN CARRIED, because kinstall's own
# `--save` is the authority on the format and a stale copy in the repo would
# drift from it. What this asserts about the format is only what it sets.
# ---------------------------------

set -u

DISK=${INSTALL_DISK:-/dev/vda}
ANSWERS=/tmp/kdos-answers
APPS=${INSTALL_APPS:-none}
CAP=${INSTALL_CAP:-5400}   # kinstall in a VM is slow and can run past 40 minutes; a tighter cap kills a healthy install
# AN ENCRYPTED INSTALL IS THE SAME RUN WITH ONE KEY CHANGED, which is the
# whole reason it is a knob here rather than a second script: what an
# encrypted install must prove is that the boot state records the CONTAINER
# beside the filesystem, and everything else about the run is identical.
#
# The passphrase is in the answer file in plain text and that is what
# kinstall's own comment says about the key: this is a throwaway virtual
# machine, and a run that stopped to be typed at is a run nothing can drive.
LUKS=${INSTALL_LUKS:-0}
LUKS_PASS=${INSTALL_LUKS_PASS:-kdosdisk}

say() { printf '\n== %s ==\n' "$*"; }

say "the target"
ls -l "$DISK" 2>&1 || { echo "no $DISK — was --disk passed?"; exit 2; }
cat /proc/partitions

# alien_apps = 0 keeps the live account's container storage out of the copy.
# `apps` takes catalogue GROUP ids. A chosen group is written to the target's
# /var/lib/kdos/apps-pending and built at the first login, not during the
# install. `none` is the default because this run exercises the install, not
# the application lane, and an empty line would read back as `essential`. An
# application id matches no group, and a line on which nothing matches falls
# back to `essential`. reboot = 0 because the rig decides when the machine
# goes down, not the installer.
say "the answer file"
cat > "$ANSWERS" <<EOF
# written by testing/install-to-disk.sh
keymap         = us
timezone       = UTC
timezone_label = UTC

disk           = $DISK
plan           = wipe
format_esp     = 1
fstype         = ext4
swap           = none
swap_mb        = 0
luks           = $LUKS
luks_passphrase = $LUKS_PASS

hostname       = kdos-test
username       = kdos
fullname       = KDOS Test
password       = kdos
root_password   = kdos
root_locked    = 0

theme          = phosphor
alien_apps     = 0
apps           = $APPS
reboot         = 0
EOF
cat "$ANSWERS"

# THE PLAN BEFORE THE POINT OF NO RETURN. `--dump plan` runs the same
# install_plan() the wizard does, so the steps and their SKIPS are the real
# ones — and it is the only look at what is about to happen that costs nothing.
say "the plan"
kinstall --config "$ANSWERS" --dump plan 2>&1 | head -40

# THE SIZE GATE, in MB as both numbers print. A payload or a disk size that
# cannot be read refuses too, since the gate cannot then say the disk is big
# enough.
say "the room"
payload=$(kinstall --config "$ANSWERS" --dump probe 2>/dev/null |
	awk '$1 == "payload" { print $2; exit }')
bytes=$(blockdev --getsize64 "$DISK" 2>/dev/null)
case "$payload$bytes" in
''|*[!0-9]*)
	echo "cannot size the run: payload '${payload}' MB, $DISK '${bytes}' bytes"
	exit 2 ;;
esac
disk=$((bytes / 1048576))
need=$((payload + 512 + 2048))
echo "payload ${payload} MB, $DISK ${disk} MB, needs ${need} MB"
[ "$disk" -ge "$need" ] || {
	echo "refusing: $DISK is ${disk} MB, the payload needs ${need} MB with the ESP and 2 GiB"
	exit 2
}

say "installing"
# THE INSTALLER GETS ITS OWN VT AND THIS SHELL KEEPS THE CONSOLE. kinstall
# owns a terminal — palette, cell buffer, evdev pointer — and pointing it at
# the serial console means its full-screen redraw is the only thing on the wire
# for as long as the install runs: no progress, no exit code, and no way to
# tell a slow copy from a wedged one. /dev/tty3 has no getty on it, so nothing
# competes for the keyboard and the UI is drawn where a person could switch to
# it with Alt+F3.
#
# THE HEARTBEAT IS THE POINT. An install is minutes of one CPU-bound step after
# another, and a run that prints nothing until it finishes cannot be told from
# a run that never will, so a cap or a person watching would kill a healthy one.
kinstall --config "$ANSWERS" --unattended < /dev/tty3 > /dev/tty3 2>&1 &
kpid=$!
beat=0
while kill -0 "$kpid" 2>/dev/null; do
	sleep 15
	beat=$((beat + 15))
	# WHAT IS USED ON THE TARGET, not partition sizes: sizes are fixed in
	# the first seconds and cannot tell a steady copy from a wedged one.
	# kinstall mounts at /mnt/kinstall; `df` on it is the one number that
	# tracks the work. AND THE INSTALLER'S OWN LAST LINE: `df` cannot say
	# which STEP is running, and a plateau is ambiguous between "finished
	# this step" and "stopped". kinstall writes every command it runs to
	# /var/log/kinstall.log, which dies with the live session's tmpfs, so
	# it is read here while it exists and printed at the end.
	printf '  %4ds  %s | %s\n' "$beat" \
	       "$(df -h /mnt/kinstall 2>/dev/null | awk 'NR==2 {printf "used %s of %s (%s)", $3, $2, $5}')" \
	       "$(tail -n 1 /var/log/kinstall.log 2>/dev/null | cut -c1-90)"
	[ "$beat" -gt "$CAP" ] && { echo "  giving up after ${CAP}s"; kill "$kpid"; break; }
done
wait "$kpid"; rc=$?
printf '\nkinstall exit=%d after %ds\n' "$rc" "$beat"

say "the installer's own log"
tail -n 40 /var/log/kinstall.log 2>/dev/null || echo "(no /var/log/kinstall.log)"

say "what landed on the disk"
cat /proc/partitions
mkdir -p /mnt/target /mnt/esp

# THE ESP IS THE HALF THAT DECIDES WHETHER IT BOOTS, and it is written at the
# END — after the rootfs copy, which is the long part. A disk with a perfect
# root filesystem and an empty ESP is exactly what a run that was cut short
# leaves behind, and the firmware's only word for it is
# `failed to load Boot0001 … Not Found`.
esp=$(ls "${DISK}"* 2>/dev/null | grep -v "^${DISK}\$" | head -1)
echo "esp partition guess: $esp"
mount "$esp" /mnt/esp 2>&1 && {
	echo "--- the ESP ---"; find /mnt/esp -maxdepth 3 | head -20
	echo "--- limine.conf ---"; cat /mnt/esp/limine.conf /mnt/esp/boot/limine/limine.conf 2>/dev/null | grep -v '^#' | head -20
	# THE BOOT STATE, AND ON AN ENCRYPTED INSTALL IT IS THE POINT. `slot_a`
	# is the FILESYSTEM and `crypt_a` is the container it is inside; the
	# kernel command line can name one `cryptdevice=`, so a second slot in
	# a second container is only reachable because each slot records its
	# own. An encrypted install whose `crypt_a` is empty is the join
	# silently not made.
	echo "--- bootstate ---"; cat /mnt/esp/EFI/kdos/bootstate 2>&1
	umount /mnt/esp
}

# THE ROOT IS A LOGICAL VOLUME. The erase plan lays an LVM group `kdos` on
# the disk (inside the LUKS container on an encrypted install) holding
# root_a, root_b, home and the swap: each slot is its own root and only /home
# is shared, with the pack store bound into each slot from
# /home/.kdos/packs. A plain partition is the root only on a disk too small
# for two slots, which the last partition then is.
root=""
if [ "$LUKS" = 1 ]; then
	crypt=$(blkid -t TYPE=crypto_LUKS -o device 2>/dev/null | grep "^${DISK}" | head -1)
	[ -n "$crypt" ] && printf '%s' "$LUKS_PASS" |
		cryptsetup open --key-file=- "$crypt" kdos-check 2>&1
fi
vgchange -ay kdos >/dev/null 2>&1 && [ -e /dev/kdos/root_a ] && root=/dev/kdos/root_a
[ -n "$root" ] || root=$(ls "${DISK}"* 2>/dev/null | grep -v "^${DISK}\$" | tail -1)
echo "--- volumes ---"; ls -l /dev/kdos/ 2>&1
echo "root: $root"
mount "$root" /mnt/target 2>&1 && {
	echo "--- top level ---"; ls /mnt/target
	echo "--- the pack store ---"; ls -la /mnt/target/var/lib/kdos/packs/ 2>&1
	echo "--- fstab (home, the packs bind, the root's own line) ---"
	cat /mnt/target/etc/fstab 2>&1 | grep -v '^#'
	grep -q '/home/.kdos/packs[[:space:]]*/var/lib/kdos/packs[[:space:]]*none[[:space:]]*bind' \
		/mnt/target/etc/fstab 2>/dev/null \
		&& echo "packs bind: present" || echo "packs bind: MISSING"
	echo "--- the user ---"; grep '^kdos' /mnt/target/etc/passwd 2>&1
	echo "--- SDDM autologin ---"
	cat /mnt/target/etc/sddm.conf.d/kde_settings.conf 2>&1
	umount /mnt/target
}
if [ -e /dev/kdos/home ] && mount /dev/kdos/home /mnt/target 2>&1; then
	echo "--- home: the shared pack store ---"; ls -la /mnt/target/.kdos/packs 2>&1
	umount /mnt/target
fi
vgchange -an kdos >/dev/null 2>&1
[ "$LUKS" = 1 ] && cryptsetup close kdos-check 2>/dev/null
sync; sync
printf '\nINSTALL DONE rc=%d\n' "$rc"
