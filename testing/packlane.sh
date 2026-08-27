#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   Has the pack lane ever run on a machine?
#
#   Runs IN THE GUEST, as root, on a booted KDOS with a desktop up:
#     testing/vnc-shot.py --root-script testing/packlane.sh
#
# WHY THIS EXISTS. Every part of the pack lane — the footer format, the solve,
# the index, the delta, the box engine, the installer's page — has been proved
# against a FIXTURE, and a fixture is a recorded machine. What none of it had
# was a kernel that mounts an erofs, a rootless podman that composes an
# overlay, a real medium to read a pack off, and a compositor to show the
# result. Those are the four things a fixture cannot be, and this is the script
# that puts a real one in front of each.
#
# IT DOES NOT STOP AT THE FIRST FAILURE. A boot of a 29 GB ISO is minutes; the
# question worth answering in one is "which of these work", not "which is the
# first that does not". Every check reports and the tally is at the end.
#
# THE ARTEFACTS IT LEAVES ARE THE POINT: /tmp/packlane/ keeps each command's
# full output, because the serial console shows a tail and a failure is usually
# explained further up.
# ---------------------------------

set -u

OUT=/tmp/packlane
mkdir -p "$OUT"

APP=${PACKLANE_APP:-app.zathura}      # 8.8 MB, recommended, wants rt-gtk
QTAPP=${PACKLANE_QTAPP:-app.kcalc}    # 2.0 MB, wants rt-qt — the other runtime
BOXBASE=${PACKLANE_BOXBASE:-alpine}   # 4.8 MB: the smallest real userland here
BOX=${PACKLANE_BOX:-packlane}
USER_NAME=${PACKLANE_USER:-kdos}

pass=0; fail=0; skip=0
n=0

say() { printf '\n== %s ==\n' "$*"; }

# Run a command, keep the whole of it, show a bounded head. `run` never
# decides anything — it is how a transcript gets read afterwards.
run() {
	n=$((n + 1))
	printf '\n$ %s\n' "$*"
	( eval "$@" ) > "$OUT/$n.log" 2>&1
	rc=$?
	head -c 1800 "$OUT/$n.log"
	printf '[rc %d, %d bytes in %s/%d.log]\n' "$rc" "$(wc -c < "$OUT/$n.log")" "$OUT" "$n"
	return $rc
}

# An assertion, with the reason on the same line as the verdict — a bare FAIL
# sends somebody back to the transcript to work out what was being asked.
check() {
	what=$1; shift
	if ( eval "$@" ) >/dev/null 2>&1; then
		pass=$((pass + 1)); printf 'PASS  %s\n' "$what"
	else
		fail=$((fail + 1)); printf 'FAIL  %s\n' "$what"
	fi
}

skipped() { skip=$((skip + 1)); printf 'SKIP  %s — %s\n' "$1" "$2"; }

asuser() { su - "$USER_NAME" -c "export XDG_RUNTIME_DIR=/run/user/1000; export WAYLAND_DISPLAY=wayland-0; $*"; }

# ── 1. the lane exists at all ──────────────────────────────────────────────
#
# erofs first, because a pack IS an erofs image and everything below this is
# unanswerable without it; then the daemon, because every verb goes through it.
say "1. the lane"

run 'uname -r; grep -c erofs /proc/filesystems || modprobe erofs; grep erofs /proc/filesystems'
check "the kernel can mount erofs"      'grep -q erofs /proc/filesystems'
check "kdos-packd is running"           'pgrep -x kdos-packd >/dev/null'
check "the socket is there"             'test -S /run/kdos-packd.sock'

run 'kdos doctor 2>&1 | sed -n "/Boxes/,/^$/p"'
run 'kdos-appbox status'
run 'ls -la /var/lib/kdos/packs/'
run 'ls /mnt/iso/packs/ | wc -l; ls /mnt/iso/packs/ | head -5'

check "the store carries a base"        'test -f /var/lib/kdos/packs/base.kpack'
check "the store carries rt-gtk"        'test -f /var/lib/kdos/packs/rt-gtk.kpack'
check "the medium carries the index"    'test -f /mnt/iso/packs/PACKAGES'
check "the medium carries $APP"         "test -f /mnt/iso/packs/$APP.kpack"
check "pack mode is the live lane"      'kdos-appbox status 2>&1 | grep -qi pack'

# ── 2. what the daemon says ────────────────────────────────────────────────
#
# `status` publishes the staging directory, the retention count and the MOUNT
# ROUTE, and the route is the one fact a fixture can never supply: with
# CONFIG_EROFS_FS_BACKED_BY_FILE the kernel takes the file directly, without it
# mount(2) answers ENOTBLK and the pack goes through a loop device.
say "2. the daemon"

run 'printf "status\n" | nc -U /run/kdos-packd.sock 2>/dev/null || kdos app sources'
run 'kdos app sources'
run 'kdos app list'
run 'kdos app list --all | head -40'
run "kdos app search zathura"
run "kdos app show $APP"

check "the catalogue is not empty"      'test "$(kdos app list --all 2>/dev/null | wc -l)" -gt 10'
check "search finds it on the medium"   "kdos app search zathura 2>&1 | grep -q ."

# ── 2b. the keyring, because a refusal here stops every verb below ─────────
#
# `kpk_verify` answers KPK_SIG_BAD both for a signature that does not check
# out and for a ring with no key that could check it — so "bad signature" is
# the message a machine gives when the pack is fine and the KEY is missing,
# which sends somebody to look at the wrong half. These four readings separate
# them: the ring's contents, the same pack read by a client that ships
# (`kdos-pack`, which reads the same /etc/kdos/keys/packs), and the same pack
# again with the ring named explicitly.
say "2b. the keyring"

run 'ls -la /etc/kdos/keys/ /etc/kdos/keys/packs/'
run 'cat /etc/kdos/keys/packs/*.pub'
run "kdos-pack info /mnt/iso/packs/$APP.kpack 2>&1 | grep -Ei '^signature|^id'"
run "kdos-pack info /var/lib/kdos/packs/base.kpack 2>&1 | grep -Ei '^signature|^id'"
run "KDOS_KEYS=/etc/kdos/keys/packs kdos-pack info /mnt/iso/packs/$APP.kpack 2>&1 | grep -i '^signature'"
run "head -3 /mnt/iso/packs/PACKAGES.sig"

check "the ring has a pack key"          'ls /etc/kdos/keys/packs/*.pub >/dev/null 2>&1'
check "a shipped client verifies a pack" "kdos-pack info /mnt/iso/packs/$APP.kpack 2>&1 | grep -q 'signed by'"

# ── 3. install one, off the medium ─────────────────────────────────────────
#
# THE CLIENT NEVER NAMES A PATH: `install` takes an id out of the list the
# daemon published, and the daemon hashes the pack where it mounts it. A pack
# on the medium has never been verified before that moment, which is why this
# is the check that matters rather than the copy that precedes it.
say "3. install $APP off the medium"

run "time kdos app install $APP"
run 'kdos app list'
run 'ls -la /var/lib/kdos/packs/'
run 'grep kdos /proc/mounts'

# ON A LIVE SESSION AN INSTALL IS A MOUNT, NOT A COPY — the pack is read off
# ISO9660 where it already is, which is the whole reason `kdos app install`
# costs nothing on a stick. So the reading that says it worked is the MOUNT,
# and a store copy here would be the bug: the same pack paid for twice.
check "$APP is mounted off the medium"  "grep -q '/mnt/iso/packs/$APP.kpack .*erofs' /proc/mounts"
check "the mount is ro,nosuid,nodev"    "grep '$APP' /proc/mounts | grep -q 'ro,nosuid,nodev'"
check "the daemon counts it mounted"    "kdos-appbox status 2>&1 | grep -q '[1-9] mounted'"

say "3b. and the Qt one, for the other runtime"
run "kdos app install $QTAPP"
check "$QTAPP is mounted too"           "grep -q '$QTAPP.kpack .*erofs' /proc/mounts"

# ── 4. the launcher, which is what a person sees ───────────────────────────
#
# `genlaunchers --packs` walks every installed app pack, mounts it through the
# daemon and parses the pack's OWN desktop entries — so a pack that installed
# and produced no launcher is an application nothing on the desktop can reach.
say "4. launchers"

run "asuser 'kdos-appbox genlaunchers --packs /' 2>&1 | tail -20"
run "asuser 'grep -c . ~/.local/share/kdos/alien-apps 2>/dev/null || echo 0'"
run "asuser 'grep zathura ~/.local/share/kdos/alien-apps /usr/share/kdos/alien-apps 2>/dev/null'"
run "ls /home/$USER_NAME/.local/share/applications/ 2>/dev/null | head -20"

check "the app table names it"          "grep -q zathura /usr/share/kdos/alien-apps"
check "the table names its pack"        "grep zathura /usr/share/kdos/alien-apps | grep -q '$APP'"
check "the shim was written"            "test -L /usr/local/bin/zathura"
check "kdos-box survived genlaunchers"  "test -x /usr/local/bin/kdos-box"
check "no launcher was dropped"         "! kdos-appbox genlaunchers --packs / 2>&1 | grep -q 'the rest are ignored'"

# ── 5. a box on a pack base ────────────────────────────────────────────────
#
# `alpine` is a base whose whole value is the image as it stands — no apt, no
# RUN, 4.8 MB — so this is the cheapest possible proof that kdos-packd composes
# an overlay and podman --rootfs runs on it.
say "5. a box on pack:$BOXBASE"

run "asuser 'kdos app install $BOXBASE'"
run "asuser 'kdos-box create $BOX base=pack:$BOXBASE accent=amber'"
run "asuser 'cat ~/.config/kdos/boxes/$BOX.conf'"
run "asuser 'kdos-box list'"
run "asuser 'kdos-box enter $BOX -- sh -c \"echo INBOX; cat /etc/os-release 2>/dev/null | head -2; id\"'"

check "the profile was written"         "test -f /home/$USER_NAME/.config/kdos/boxes/$BOX.conf"
check "the box runs a command"          "asuser 'kdos-box enter $BOX -- echo INBOX' 2>&1 | grep -q INBOX"

# ── 6. W6-6 — does the telemetry NAME a user box ───────────────────────────
#
# Every one of these identifies a box by walking the ppid chain to the conmon
# that supervises it and reading `-n <name>`. That is supposed to pick up a box
# nobody has heard of with no change, and this is the task that PROVES it
# rather than the one that builds it: a spinner inside `$BOX` must be reported
# as `<proc> (appbox $BOX)` and not as the default box, and not unnamed.
say "6. telemetry names the box"

asuser "kdos-box enter $BOX -- sh -c 'while :; do :; done' >/dev/null 2>&1 &" >/dev/null 2>&1
sleep 8
run "pgrep -af conmon | head -5"
run "asuser 'kdos-res --page boxes --dump --dump-size 132x43'"

check "conmon supervises the box"       "pgrep -af conmon | grep -q '$BOX'"
check "kdos-res Boxes lists it"         "asuser 'kdos-res --page boxes --dump' 2>&1 | grep -q '$BOX'"

# THE TWO DAEMONS ARE ENVIRONMENTAL AND ARE REPORTED AS SUCH. kdos-energyd
# refuses to start where there is no readable RAPL domain, which is most VMs,
# and `kdos stutter` has nothing to join without the compositor's frames
# socket. A machine that cannot answer is a SKIP with the reason on the line;
# calling it a failure would make every VM look like a broken distro, and
# calling it a pass would be a green line for something never tested.
if pgrep -x kdos-energyd >/dev/null; then
	run "timeout 20 kdos-energy 2>&1 | head -25"
	check "kdos-energy names the box"   "timeout 20 kdos-energy 2>&1 | grep -q '$BOX'"
else
	skipped "kdos-energy names the box" "no kdos-energyd — this machine publishes no RAPL domain"
fi

if [ -S "/run/user/1000/kdos-frames.sock" ]; then
	run "timeout 25 kdos stutter 2>&1 | head -30"
	skipped "kdos stutter names the box" "a stutter has to HAPPEN; the transcript is the evidence"
else
	skipped "kdos stutter names the box" "no frames socket — no compositor is reporting"
fi

pkill -f 'while :; do :; done' 2>/dev/null

# ── 7. launch an installed pack app, and time it ───────────────────────────
#
# The cold start is the number W9-4's checkpoint asks for, and the trace file
# is where kdos-appbox already records its stages — reading it is cheaper and
# more honest than timing a wrapper from outside.
say "7. cold start"

run "asuser 'rm -f \$XDG_RUNTIME_DIR/kdos-appbox.trace; timeout 45 zathura >/dev/null 2>&1 & sleep 30; pgrep -x zathura >/dev/null && echo ALIVE || echo GONE'"
run "asuser 'cat \$XDG_RUNTIME_DIR/kdos-appbox.trace 2>/dev/null'"
run "asuser 'kdos hey boxes 2>&1; kdos hey list 2>&1 | head -20'"

# ── 8. the tally ───────────────────────────────────────────────────────────
say "tally"
printf 'pass %d   fail %d   skip %d\n' "$pass" "$fail" "$skip"
printf 'transcripts in %s (%d commands)\n' "$OUT" "$n"
[ "$fail" = 0 ] && printf 'PACKLANE OK\n' || printf 'PACKLANE INCOMPLETE\n'
