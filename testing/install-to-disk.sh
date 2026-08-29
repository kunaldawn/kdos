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
# WHY. Half of what the pack lane does cannot happen on a live session at all:
# $HOME is on overlayfs, an overlay upper cannot sit there, and so no box can
# ever START. `kdos doctor` says so and it is right. Everything gated behind
# "needs an installed system" — a box that runs, `kdos-box freeze`, telemetry
# naming a running box, the installer's own Applications page — needs a machine
# that was installed, and this is the cheapest way to get one.
#
# THE ANSWER FILE IS WRITTEN HERE RATHER THAN CARRIED, because kinstall's own
# `--save` is the authority on the format and a stale copy in the repo would
# drift from it. What this asserts about the format is only what it sets.
# ---------------------------------

set -u

DISK=${INSTALL_DISK:-/dev/vda}
ANSWERS=/tmp/kdos-answers
PACKS=${INSTALL_PACKS:-"app.zathura app.kcalc alpine"}
CAP=${INSTALL_CAP:-5400}   # kinstall in a VM is slow; 40 minutes was not enough

say() { printf '\n== %s ==\n' "$*"; }

say "the target"
ls -l "$DISK" 2>&1 || { echo "no $DISK — was --disk passed?"; exit 2; }
cat /proc/partitions

# alien_apps = 0 is deliberate: copying the 3.9 GB monolith is the OTHER lane
# and this run exists to exercise packs. reboot = 0 because the rig decides
# when the machine goes down, not the installer.
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
luks           = 0

hostname       = kdos-test
username       = kdos
fullname       = KDOS Test
password       = kdos
root_password   = kdos
root_locked    = 0

theme          = phosphor
alien_apps     = 0
packs          = $PACKS
reboot         = 0
EOF
cat "$ANSWERS"

# THE PLAN BEFORE THE POINT OF NO RETURN. `--dump plan` runs the same
# install_plan() the wizard does, so the steps and their SKIPS are the real
# ones — and it is the only look at what is about to happen that costs nothing.
say "the plan"
kinstall --config "$ANSWERS" --dump plan 2>&1 | head -40

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
# another; a run that prints nothing until it finishes cannot be told from a
# run that never will, which is exactly how the first attempt was killed while
# it was still working.
kinstall --config "$ANSWERS" --unattended < /dev/tty3 > /dev/tty3 2>&1 &
kpid=$!
beat=0
while kill -0 "$kpid" 2>/dev/null; do
	sleep 15
	beat=$((beat + 15))
	# WHAT IS USED ON THE TARGET, not what the partition table says. The
	# first version printed partition SIZES, which are decided in the first
	# thirty seconds and never move again — so a run that was copying
	# steadily and a run that was wedged printed the same line, which is
	# the failure the heartbeat exists to prevent. kinstall mounts at
	# /mnt; `df` on it is the one number that tracks the work.
	# AND THE INSTALLER'S OWN LAST LINE. `df` says whether anything is being
	# written; it cannot say which STEP is running, and a plateau is
	# ambiguous between "finished this step" and "stopped". kinstall writes
	# every command it runs to /var/log/kinstall.log through `logf_`, and
	# that file dies with the live session's tmpfs — so it is read here,
	# while it exists, and copied out at the end.
	printf '  %4ds  %s | %s\n' "$beat" \
	       "$(df -h /mnt 2>/dev/null | awk 'NR==2 {printf "used %s of %s (%s)", $3, $2, $5}')" \
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
	echo "--- refind.conf ---"; cat /mnt/esp/EFI/*/refind.conf 2>/dev/null | grep -v '^#' | head -20
	umount /mnt/esp
}

part=$(ls "${DISK}"* 2>/dev/null | grep -v "^${DISK}\$" | tail -1)
echo "root partition guess: $part"
mount "$part" /mnt/target 2>&1 && {
	echo "--- top level ---"; ls /mnt/target
	echo "--- the pack store ---"; ls -la /mnt/target/var/lib/kdos/packs/ 2>&1
	echo "--- fstab ---"; cat /mnt/target/etc/fstab 2>&1 | grep -v '^#'
	echo "--- the user ---"; grep '^kdos' /mnt/target/etc/passwd 2>&1
	echo "--- inittab autologin ---"; grep -i autologin /mnt/target/etc/inittab 2>&1
	umount /mnt/target
}
sync; sync
printf '\nINSTALL DONE rc=%d\n' "$rc"
