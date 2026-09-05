#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   Put freshly built binaries onto a booted ISO, and restart the session.
#
# RUNS IN THE GUEST, as root, from `testing/quick.sh`'s --root-script. The tar
# arrives on a raw virtio disk that vnc-shot.py attached with --data-disk: no
# partition table and no filesystem, so it is read with `dd` straight off the
# device.
#
# THIS EXISTS BECAUSE THE ISO STEP IS SIX MINUTES AND A BINARY IS TWO HUNDRED
# KILOBYTES. Rebuilding the image to look at a changed program made every
# iteration twelve minutes, nearly all of it repacking 32 GB that did not
# change. The live medium's root is an overlay in RAM, so untarring over it is
# both allowed and thrown away at the next boot — which is the property that
# makes this safe rather than a way to corrupt a test image.
#
# THE SESSION IS RESTARTED, NOT THE MACHINE. Rebooting would lose the patch —
# it lives only in the RAM overlay — so the chain is ended and started again.
#
# A RESTARTED SESSION IS NOT A BOOTED ONE, and one difference is measured and
# has cost hours: anything that owns a D-BUS NAME does not come back cleanly.
# `kdos-notifyd` is killed here for that reason, and even so a notification
# raised in a restarted session has never been seen to draw, while the same
# call on a booted ISO does. **Verify anything involving the bus on a real
# boot.** What this loop is good for is a surface's own drawing, a chord, a
# window rule, a terminal's behaviour — everything the session and its clients
# do among themselves.
# ---------------------------------
set -u

# Substituted by quick.sh: the guest inherits nothing from the host, so
# the choices are baked into the copy that is sent. They are declared
# HERE, above every reader — `set -u` makes a use before this point an
# abort, which ends the script silently in the middle of a patch.
KEEP=0
SKEL=0

log() { echo "quickpatch: $*"; }

# ── find the disk carrying the tar ──────────────────────────────────────
#
# By CONTENT, not by name. Which /dev/vdX the data disk lands on depends on how
# many drives the invocation attached, and a hardcoded letter is a patch
# applied to whatever else was mounted there.
disk=""
for d in /dev/vd?; do
	[ -b "$d" ] || continue
	if dd if="$d" bs=1 skip=257 count=5 2>/dev/null | grep -q ustar; then
		disk=$d
		break
	fi
done
if [ -z "$disk" ]; then
	log "no data disk carrying a tar — nothing patched"
	exit 0
fi
log "patch on $disk"

# The raw drive is rounded up to a sector boundary, so the tar is followed by
# whatever was in those bytes. tar stops at its own end marker and never looks,
# which is why this needs no length and no truncate.
if ! dd if="$disk" bs=1M 2>/dev/null | tar xf - -C / 2>/tmp/quickpatch.err; then
	log "untar failed:"
	cat /tmp/quickpatch.err
	exit 1
fi
log "unpacked"

# ── THE LIVE USER'S COPY OF A SKEL FILE ─────────────────────────────────
#
# `/etc/skel` seeds a NEW account and the desktop user's home was seeded at
# install time, so a chord table patched in skel changes nothing for the person
# logged in. When the tar carried one, copy it over — this is a test harness on
# a medium whose root is thrown away at the next boot, and the alternative is a
# config change that cannot be tested at all without a full build.
if [ -d /etc/skel/.config ] && [ "$SKEL" = 1 ]; then
	cp -r /etc/skel/.config/. /home/kdos/.config/ 2>/dev/null
	chown -R kdos:kdos /home/kdos/.config 2>/dev/null
	log "skel copied over the live user's config"
fi

# ── restart the console session ─────────────────────────────────────────
#
# All of them, and in this order. The session and its view OUTLIVE the login
# shell by design, and the view holds DRM master — so ending only the login
# chain leaves the old view owning the screen and the new session drawing onto
# one it does not have.
#
# THE WAIT IS FOR A DIFFERENT PID, not for the socket. The old session's socket
# file outlives the process that bound it, so "the socket is there" is true one
# millisecond after the kill and the steps then run against the binaries this
# script just replaced.
if [ "$KEEP" = 1 ]; then
	log "session left alone — the new binaries run when something spawns them"
	sync
	exit 0
fi

old=$(pgrep -x kdos-con | head -1)
log "old session pid ${old:-none}"

pkill -x kdos-view      2>/dev/null
pkill -x kdos-con       2>/dev/null
pkill -f kdos-con-start 2>/dev/null

# AND THE SHELL SURFACES, which the session does not own. They are not
# supervised and they outlive it — and `kdos-notifyd` holds a BUS NAME, so a
# stale one keeps answering `Notify` while drawing to a session that is gone
# and the new one exits because the name is taken. The symptom is a
# notification that returns an id and never appears, which reads exactly like
# a bug in whatever raised it.
#
# `kdos-shell` IS THE PANEL and is in this list for the same reason: a stale
# one keeps its bar docked, so the session's fallback stays stood down and
# every photograph shows the binary that was replaced.
for _s in kdos-notifyd kdos-desk kdos-ime kdos-slit kdos-shell; do
	pkill -x "$_s" 2>/dev/null
done
sleep 1

# THE STALE SOCKETS GO WITH IT, and this is not tidiness. `kdos-con-start`'s
# readiness test is "does con.view exist", and the file outlives the process
# that bound it — so on a restart it returns TRUE immediately and the icon
# layer and the notification daemon are started before the new session has
# bound anything. They are not supervised, so they fail once and stay gone,
# and the desktop comes back with no icons and no toasts for reasons that look
# like the code under test.
rm -f /run/user/1000/kdos/con.sock /run/user/1000/kdos/con.view

# ── AND START IT AGAIN BY HAND ──────────────────────────────────────────
#
# NOT by waiting for init. `/etc/inittab` respawns tty1, and it does not come
# back here: ending the chain takes the getty with it and init leaves it down —
# measured, twice, with nothing but `getty` on tty2 left running. Rather than
# guess at a respawn policy, this starts the same program the getty would:
# `kdos-con-login` IS `kdos-con` under another name (a basename dispatch), and
# it does the autologin and execs the supervisor exactly as it does at boot.
#
# On tty1's own descriptors, because the session opens the seat for that
# terminal and a process with no controlling terminal there gets no input.
setsid /usr/local/sbin/kdos-con-login tty1 </dev/tty1 >/dev/tty1 2>&1 &

i=0
while [ $i -lt 80 ]; do
	now=$(pgrep -x kdos-con | head -1)
	if [ -n "$now" ] && [ "$now" != "${old:-}" ] &&
	   [ -S /run/user/1000/kdos/con.view ]; then
		# And a moment for the once-per-login block to attach the icon
		# layer and the notification daemon, which are started after
		# the session is accepting rather than before.
		sleep 2
		log "session $now up after $((i * 5))00ms"
		sync
		exit 0
	fi
	sleep 0.5
	i=$((i + 1))
done
log "the session did not come back — what is running:"
ps -eo pid,comm | grep -E 'kdos|getty' | head -12
log "and the supervisor said:"
tail -5 /run/user/1000/kdos-con.log 2>/dev/null
sync
exit 1
