#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/session-cost.sh — what the desktop costs: time to the panel, and
#   memory at idle
#
#   testing/session-cost.sh [--gl] [--settle SECONDS] [--user NAME] \
#                           [-- vnc-shot args...]
#
# Boots build/iso-build/kdos.iso on the rig, measures from inside the guest,
# and prints a block of `session-cost:` lines that testing.md records per
# release. Run on the host; it starts the rig container itself. The numbers are
# also written to build/session-cost.txt. --gl boots with virtio-vga-gl, the
# configuration the timing budget is set for; it needs the GPU passed into the
# container (--gpus all, /dev/dri), which this adds.
#
# TIMES ARE SECONDS SINCE THE KERNEL STARTED, from /proc. A process's start
# time is in its /proc/<pid>/stat whenever it is read, so everything that is
# still running when the guest script looks is exact however late it looks:
# the display manager, the greeter, the session, KWin, the shell. Two events
# leave no process behind and are timed by watching, every quarter second from
# the moment the serial console has a shell: KSplash's last stage
# (setStage("desktop") on org.kde.KSplash, seen by dbus-monitor) and
# org.kde.plasmashell becoming owned. Where the watch began after an event,
# its line says `<=` and the time is when it was first seen. "rcS" is the
# oldest process rcS started that still runs, because rcS itself has exited.
#
#   kernel → rcS → display manager → greeter → session → KSplash done
#                                                       → shell on the bus
#
# MEMORY IS PSS, from /proc/<pid>/smaps_rollup, read --settle seconds (60)
# after the panel is up: the session's processes (every process of the
# account) and the whole system's (every process with a command line, kernel
# threads excluded). PSS divides each shared page among the processes mapping
# it, so the sums add up to what the processes cost together and a library
# shared by fifty of them is counted once. The fifteen largest are listed.
#
# The guest half runs as root through vnc-shot.py's --root-script:
#   testing/session-cost.sh --guest [--settle S] [--user NAME] [--limit S]
# ---------------------------------
set -u

if [ "${1:-}" != --guest ]; then
	gl=0
	settle=60
	user=kdos
	while [ $# -gt 0 ]; do
		case $1 in
		--gl) gl=1 ;;
		--settle) settle=$2; shift ;;
		--user) user=$2; shift ;;
		--) shift; break ;;
		*) echo "session-cost: unknown option $1" >&2; exit 64 ;;
		esac
		shift
	done
	cd "$(dirname "$0")/.." || exit 1
	[ -f build/iso-build/kdos.iso ] || {
		echo "session-cost: no build/iso-build/kdos.iso — run a full build once" >&2
		exit 1
	}
	mkdir -p build/shots
	set -- --until shell \
		--root-script "testing/session-cost.sh --guest --settle $settle --user $user" \
		--shot /kdos/build/shots/session-cost.ppm "$@"
	dev="--device /dev/kvm"
	if [ "$gl" = 1 ]; then
		dev="$dev --gpus all --device /dev/dri"
		[ -c /dev/udmabuf ] && dev="$dev --device /dev/udmabuf"
		set -- --gl "$@"
	fi
	# The rig's status outlives the pipe through tee: a root script given
	# after `--` (the session probe, from `make session-probe`) fails the run
	# through it.
	rm -f build/session-cost.rc
	{
		# shellcheck disable=SC2086
		docker run --rm $dev -v "$PWD:/kdos" -w /kdos kdos-qemu-py:latest \
			python3 testing/vnc-shot.py --size 1920x1080 "$@"
		echo $? > build/session-cost.rc
	} 2>&1 | tee build/session-cost.log
	grep '^session-cost:' build/session-cost.log > build/session-cost.txt || true
	echo "==> build/session-cost.txt"
	cat build/session-cost.txt
	read -r rc < build/session-cost.rc || rc=1
	grep -q '^session-cost: result=ok' build/session-cost.txt || rc=1
	exit "$rc"
fi

# ── in the guest ────────────────────────────────────────────────────────
shift
settle=60
user=kdos
limit=300
while [ $# -gt 0 ]; do
	case $1 in
	--settle) settle=$2; shift ;;
	--user) user=$2; shift ;;
	--limit) limit=$2; shift ;;
	*) echo "session-cost: unknown option $1"; exit 64 ;;
	esac
	shift
done

uid=$(id -u "$user" 2>/dev/null) || { echo "session-cost: result=fail no account $user"; exit 1; }
gid=$(id -g "$user")
as_user() { setpriv --reuid="$uid" --regid="$gid" --init-groups "$@"; }
addr=unix:path=/run/user/$uid/bus
work=/run/kdos-rig/cost.$$
mkdir -p "$work"
trap 'kill "$mon" 2>/dev/null; rm -rf "$work"' EXIT
mon=""

hz=$(getconf CLK_TCK 2>/dev/null)
case $hz in '' | *[!0-9]*) hz=100 ;; esac
now() { read -r _t _ < /proc/uptime; echo "$_t"; }
# When a process started, in seconds since the kernel did. The command name
# can hold spaces and parentheses, so the fields are counted after its last ')'.
pstart() {
	[ -n "$1" ] || return 0
	awk -v hz="$hz" '{ sub(/^.*\) /, ""); printf "%.2f\n", $20 / hz }' \
		"/proc/$1/stat" 2>/dev/null
}
oldest() { pgrep -o "$@" 2>/dev/null; }
put() { echo "$1=$2" >> "$work/t"; }
get() { sed -n "s/^$1=//p" "$work/t" 2>/dev/null | head -n 1; }
: > "$work/t"

# The desktop starts with SDDM; until it does there is nothing to time.
desktop=""
t0=$(now)
first=1
while :; do
	if [ -z "$desktop" ]; then
		if pgrep -x sddm >/dev/null 2>&1; then desktop=plasma; fi
	fi
	case $desktop in
	plasma)
		if [ -z "$mon" ] && [ -S "/run/user/$uid/bus" ]; then
			setpriv --reuid="$uid" --regid="$gid" --init-groups dbus-monitor --address "$addr" \
				"type='method_call',interface='org.kde.KSplash',member='setStage'" \
				> "$work/ksplash" 2>&1 &
			mon=$!
		fi
		if [ -z "$(get splash_done)" ] &&
			grep -q 'string "desktop"' "$work/ksplash" 2>/dev/null; then
			put splash_done "$(now)"
			[ "$first" = 1 ] && put splash_done_late 1
		fi
		if [ -z "$(get shell_owned)" ] && [ -S "/run/user/$uid/bus" ] &&
			as_user dbus-send --bus="$addr" --print-reply=literal \
				--dest=org.freedesktop.DBus /org/freedesktop/DBus \
				org.freedesktop.DBus.NameHasOwner string:org.kde.plasmashell \
				2>/dev/null | grep -q true; then
			put shell_owned "$(now)"
			[ "$first" = 1 ] && put shell_owned_late 1
		fi
		if [ -n "$(get shell_owned)" ]; then
			if [ -n "$(get splash_done)" ]; then break; fi
			if ! pgrep -u "$uid" -x ksplashqml >/dev/null 2>&1; then
				# No splash ran, or it ended before the watch could see its
				# stages: the panel time is the shell's.
				put splash_gone "$(now)"
				break
			fi
		fi
		;;
	esac
	first=0
	if awk -v a="$t0" -v b="$(now)" -v l="$limit" 'BEGIN { exit !(b - a > l) }'; then
		echo "session-cost: result=fail no ${desktop:-desktop} after ${limit}s"
		pgrep -a 'sddm|kwin_wayland|plasmashell|ksplashqml|kdos-' 2>/dev/null
		exit 1
	fi
	sleep 0.25
done
kill "$mon" 2>/dev/null

# The oldest process with a command line other than init: rcS's first service
# that is still up. A kernel thread has an empty command line. Every file in
# /proc reports a size of 0, so emptiness is read, not tested with -s.
userland() { grep -q . "$1/cmdline" 2>/dev/null; }
rcs_pid=""
rcs=""
for d in /proc/[0-9]*; do
	p=${d#/proc/}
	[ "$p" = 1 ] && continue
	userland "$d" || continue
	s=$(pstart "$p")
	[ -n "$s" ] || continue
	if [ -z "$rcs" ] || awk -v a="$s" -v b="$rcs" 'BEGIN { exit !(a < b) }'; then
		rcs=$s
		rcs_pid=$p
	fi
done
rcs_name=""
[ -n "$rcs_pid" ] && rcs_name=$(tr '\0' ' ' < "/proc/$rcs_pid/cmdline" 2>/dev/null | cut -c1-60)

line() { printf 'session-cost: %s\n' "$*"; }
line "desktop=$desktop"
line "t_rcs=$rcs (oldest rcS service still running: $rcs_name)"
case $desktop in
plasma)
	t_dm=$(pstart "$(oldest -x sddm)")
	t_greeter=$(pstart "$(oldest -u sddm -f '^[^ ]*sddm-greeter')")
	t_session=$(pstart "$(oldest -u "$uid" -x startplasma-way)")
	[ -n "$t_session" ] || t_session=$(pstart "$(oldest -u "$uid" -f '^[^ ]*startplasma-wayland')")
	t_kwin=$(pstart "$(oldest -u "$uid" -x kwin_wayland)")
	t_shell=$(pstart "$(oldest -u "$uid" -x plasmashell)")
	t_splash=$(get splash_done)
	t_owned=$(get shell_owned)
	line "t_dm=$t_dm (sddm)"
	if [ -n "$t_greeter" ]; then line "t_greeter=$t_greeter (sddm-greeter)"
	else line "t_greeter= (no greeter: autologin)"; fi
	line "t_session=$t_session (startplasma-wayland)"
	line "t_kwin=$t_kwin (the session's kwin_wayland)"
	line "t_shell_start=$t_shell (plasmashell)"
	late=""
	[ -n "$(get shell_owned_late)" ] && late="<="
	line "t_shell_owned=$late$t_owned (org.kde.plasmashell on the bus)"
	if [ -n "$t_splash" ]; then
		late=""
		[ -n "$(get splash_done_late)" ] && late="<="
		line "t_splash_done=$late$t_splash (KSplash setStage desktop)"
		panel=$t_splash
	else
		line "t_splash_done= (no stage seen; KSplash gone at $(get splash_gone))"
		panel=$t_owned
	fi
	awk -v r="$rcs" -v d="$t_dm" -v s="$t_session" -v p="$panel" -v o="$t_owned" 'BEGIN {
		printf "session-cost: rcs_to_dm=%.2f\n", d - r
		printf "session-cost: login_to_panel=%.2f\n", p - s
		printf "session-cost: login_to_shell_owned=%.2f\n", o - s
		printf "session-cost: boot_to_panel=%.2f\n", p
	}'
	;;
esac

# ── idle memory ─────────────────────────────────────────────────────────
echo "session-cost: settling ${settle}s before reading memory"
sleep "$settle"
: > "$work/pss"
for d in /proc/[0-9]*; do
	userland "$d" || continue
	k=$(awk '/^Pss:/ { print $2; exit }' "$d/smaps_rollup" 2>/dev/null)
	[ -n "$k" ] || continue
	u=$(awk '/^Uid:/ { print $2; exit }' "$d/status" 2>/dev/null)
	c=$(cat "$d/comm" 2>/dev/null)
	printf '%s %s %s %s\n' "$k" "$u" "${d#/proc/}" "$c" >> "$work/pss"
done
awk -v uid="$uid" '
	{ all += $1; n++ } $2 == uid { mine += $1; m++ }
	END {
		printf "session-cost: pss_session_kib=%d (%d processes of uid %d)\n", mine, m, uid
		printf "session-cost: pss_system_kib=%d (%d processes)\n", all, n
	}' "$work/pss"
echo "session-cost: largest (PSS KiB, uid, pid, name):"
sort -rn "$work/pss" | head -n 15 | sed 's/^/session-cost:   /'
line "result=ok"
