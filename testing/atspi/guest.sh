#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/atspi/guest.sh — dump KDOS surfaces' AT-SPI trees on a booted ISO
#
#   vnc-shot.py --data-disk IN.tar --scratch OUT.img \
#       --root-script 'testing/atspi/guest.sh [--webdriver] [--user U] SURFACE...'
#
# RUNS IN THE GUEST, AS ROOT, once vnc-shot.py's gate has seen the Plasma
# session up; testing/atspi/golden.sh is what starts it and reads the result.
# The harness itself (atspi.py and the surface files) comes in on a data disk
# as a tar of testing/atspi, and the dumps go back out as a tar written over the
# scratch disk — the only path out of a guest with no network. Both disks are
# raw, and both are found by what is on them, never by their letter: the data
# disk holds a tar, and the scratch disk starts with golden.sh's marker.
#
# Each surface is dumped by atspi.py running as the session's account with
# plasmashell's own environment — the session's bus, display and config
# directories — so the program it starts is the one a person would get from
# Kickoff. The a11y bus is told IsEnabled first, which is what makes a Qt
# program publish its tree at all. With --webdriver the dumps go through
# selenium-webdriver-at-spi, served from its installed copy for the run;
# without it the bus is read directly, which is the route goldens are
# written from.
# ---------------------------------
set -u

user=kdos
webdriver=0
while [ $# -gt 0 ]; do
	case $1 in
	--user) user=$2; shift ;;
	--webdriver) webdriver=1 ;;
	--) shift; break ;;
	-*) echo "atspi-guest: unknown option $1"; exit 64 ;;
	*) break ;;
	esac
	shift
done

say() { echo "atspi-guest: $*"; }
uid=$(id -u "$user" 2>/dev/null) || { say "no account $user"; exit 1; }
gid=$(id -g "$user")
work=/run/kdos-rig/atspi
out=/run/kdos-rig/atspi-out
rm -rf "$work" "$out"
mkdir -p "$work" "$out"
chmod 0755 /run/kdos-rig "$work"
chown "$uid:$gid" "$out"

# ── the two disks ───────────────────────────────────────────────────────
in=""
scratch=""
for d in /dev/vd?; do
	[ -b "$d" ] || continue
	if dd if="$d" bs=1 skip=257 count=5 2>/dev/null | grep -q ustar; then
		in=$d
	elif [ "$(dd if="$d" bs=16 count=1 2>/dev/null)" = KDOS-RIG-SCRATCH ]; then
		scratch=$d
	fi
done
[ -n "$in" ] || { say "no data disk carrying the harness"; exit 1; }
[ -n "$scratch" ] || { say "no scratch disk (golden.sh marks it KDOS-RIG-SCRATCH)"; exit 1; }
dd if="$in" bs=1M 2>/dev/null | tar xf - -C "$work" || { say "could not unpack the harness"; exit 1; }
harness=$work/testing/atspi
[ -f "$harness/atspi.py" ] || { say "the tar holds no testing/atspi/atspi.py"; exit 1; }
chmod -R a+rX "$work"

if [ $# -eq 0 ]; then
	# Surface names are file names of [A-Za-z0-9_.-], so they split cleanly.
	# shellcheck disable=SC2046
	set -- $(python3 -I "$harness/atspi.py" list)
fi

# ── the session ─────────────────────────────────────────────────────────
shell=$(pgrep -u "$uid" -x plasmashell | head -n 1)
[ -n "$shell" ] || { say "no plasmashell of $user: no session to dump in"; exit 1; }
cp "/proc/$shell/environ" "$work/environ"

# As the account, with the session's environment: xargs hands the
# NUL-separated variables over as arguments, so no value is read by a shell.
# RIG_CMD is the command, word-split; it holds no user data.
in_session() {
	RIG_CMD=$*
	export RIG_CMD
	# shellcheck disable=SC2016
	xargs -0 sh -c 'u=$1 g=$2; shift 2
		exec setpriv --reuid="$u" --regid="$g" --init-groups \
			env -i "$@" $RIG_CMD' atspi "$uid" "$gid" < "$work/environ"
}

in_session dbus-send --session --print-reply --dest=org.a11y.Bus /org/a11y/bus \
	org.freedesktop.DBus.Properties.Set string:org.a11y.Status \
	string:IsEnabled variant:boolean:true >/dev/null 2>&1 ||
	say "could not set org.a11y.Status IsEnabled; dumping anyway"

url=""
if [ "$webdriver" = 1 ]; then
	server=/usr/share/selenium-webdriver-at-spi
	[ -f "$server/selenium-webdriver-at-spi.py" ] ||
		{ say "selenium-webdriver-at-spi is not installed ($server)"; exit 1; }
	(cd "$server" && in_session env FLASK_APP=selenium-webdriver-at-spi.py \
		GDK_BACKEND=wayland python3 -m flask run --port 4723 --no-reload \
		> "$out/webdriver.log" 2>&1) &
	url=http://127.0.0.1:4723
	i=0
	until python3 -I -c "import urllib.request; urllib.request.urlopen('$url/status', timeout=2)" \
		>/dev/null 2>&1; do
		i=$((i + 1))
		[ $i -lt 40 ] || { say "the WebDriver never answered"; exit 1; }
		sleep 0.5
	done
	say "selenium-webdriver-at-spi answers at $url"
fi

# ── the dumps ───────────────────────────────────────────────────────────
fails=0
for s in "$@"; do
	case $s in
	*[!A-Za-z0-9_.-]*) say "skipping a surface name with odd characters: $s"; continue ;;
	esac
	wd=""
	[ -n "$url" ] && wd="--webdriver $url"
	# shellcheck disable=SC2086 # $wd is two words or none
	if in_session python3 -I "$harness/atspi.py" dump "$s" $wd \
		--out "$out/$s.txt" 2> "$out/$s.err"; then
		say "dumped $s ($(grep -c . "$out/$s.txt") lines)"
	else
		say "FAILED $s: $(tail -n 1 "$out/$s.err")"
		fails=$((fails + 1))
	fi
done
[ -n "$url" ] && pkill -u "$uid" -f "flask run --port 4723" 2>/dev/null

# Over the scratch disk from offset 0; the host reads it with a plain tar,
# which stops at the archive's own end marker.
tar cf "$scratch" -C "$out" . && sync
say "$# surface(s), $fails failed; dumps written to $scratch"
exit "$fails"
