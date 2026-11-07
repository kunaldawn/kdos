#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/probe.sh — one login-flow assertion on a booted KDOS
#
#   testing/vnc-shot.py --root-script 'testing/flows/probe.sh [OPTIONS] CHECK [ARG]'
#
#     --user NAME      the session's account (kdos)
#     --timeout SECS   how long a check waits for its state (180)
#
#   CHECK
#     session [SERVICE]  the account's Plasma session is up: org.kde.plasmashell
#                        owned on its bus, KSplash gone, and logind's session of
#                        it Active, Wayland, class user, on seat0. With SERVICE,
#                        the session was opened by that PAM service: sddm for a
#                        typed password, sddm-autologin for an autologin
#     greeter            SDDM's greeter is the active session on seat0 and the
#                        account has no active session
#     logout             ask ksmserver to log the account out (org.kde.Shutdown
#                        logout, the call the Log Out button makes), then
#                        `greeter`
#     locked | unlocked  org.freedesktop.ScreenSaver GetActive on the
#                        account's bus, and logind's LockedHint, agree
#     suspend [SECS]     arm the RTC to wake the machine SECS (20) from now,
#                        suspend through logind, and wait for the kernel's
#                        suspend counter to move: the machine slept and woke
#     vt N               tty N is the foreground VT. For the session's own VT
#                        the session is Active, for any other it is not, and
#                        tty2 has its getty
#     clean PATH         remove PATH, before a keystroke test
#     no-leak PATH       PATH does not exist, the session VT's keyboard is in
#                        K_OFF mode and its tty holds no unread input: no key
#                        typed into the session reached a line discipline
#
# RUNS IN THE GUEST, AS ROOT, sent down the serial console by vnc-shot.py's
# --root-script. Every assertion prints one line, `ok` or `FAIL` and what it
# looked at; on a failure the sessions, the processes and the tails of SDDM's
# and the session's logs follow. The exit status is the number of failed
# assertions, so vnc-shot.py fails the run on anything but 0.
#
# EVERY STATE IS ASKED, NEVER PHOTOGRAPHED. logind (elogind) answers which
# session is active, of which class, opened by which PAM service and locked or
# not; the session bus answers whether the shell and the screen locker are
# up; the kernel answers which VT is in front, what the VT keyboard is set to,
# and how many suspends have completed. A picture of a greeter and a picture
# of a black frame with a greeter's colours cannot be told apart this way.
#
# THE SESSION BUS IS ASKED AS THE ACCOUNT. A session bus drops root's
# connection before Hello, so every call on it runs under setpriv with the
# account's ids; setpriv opens no PAM session of its own.
# ---------------------------------
# The state functions are called only by name, through until_true.
# shellcheck disable=SC2329
set -u

user=kdos
limit=180
while [ $# -gt 0 ]; do
	case $1 in
	--user) user=$2; shift ;;
	--timeout) limit=$2; shift ;;
	-h | --help) sed -n '9,38p' "$0"; exit 0 ;;
	-*) echo "flow-probe: unknown option $1" >&2; exit 64 ;;
	*) break ;;
	esac
	shift
done
[ $# -ge 1 ] || { echo "flow-probe: no check named" >&2; exit 64; }
check=$1
arg=${2:-}

fails=0
ok() { echo "ok    $*"; }
bad() { fails=$((fails + 1)); echo "FAIL  $*"; }
info() { echo "      $*"; }

uid=$(id -u "$user" 2>/dev/null) || { echo "FAIL  no account $user"; exit 1; }
gid=$(id -g "$user")
as_user() { setpriv --reuid="$uid" --regid="$gid" --init-groups "$@"; }
home=$(awk -F: -v u="$user" '$1 == u { print $6 }' /etc/passwd)
addr=unix:path=/run/user/$uid/bus

# One call on the account's session bus; literal output, nothing on failure.
sbus() {
	_dest=$1 _path=$2 _method=$3
	shift 3
	[ -S "/run/user/$uid/bus" ] || return 1
	as_user dbus-send --bus="$addr" --print-reply=literal --reply-timeout=10000 \
		--dest="$_dest" "$_path" "$_method" "$@" 2>/dev/null
}
owned() {
	sbus org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner \
		string:"$1" | grep -q true
}

# Poll a test until it holds or the limit passes. The test is a function name
# and its arguments, run in this shell, which is the only way the state
# functions below are called.
now() { read -r _t _ < /proc/uptime; echo "${_t%.*}"; }
until_true() {
	_end=$(($(now) + limit))
	while :; do
		"$@" && return 0
		[ "$(now)" -ge "$_end" ] && return 1
		sleep 1
	done
}

# logind's sessions, one id per line, and one property of one.
sessions() { loginctl list-sessions --no-legend 2>/dev/null | awk '{ print $1 }'; }
prop() { loginctl show-session "$1" -p "$2" --value 2>/dev/null; }
# The account's graphical session on seat0: the newest one of class user.
user_session() {
	for _s in $(sessions); do
		[ "$(prop "$_s" Name)" = "$user" ] || continue
		[ "$(prop "$_s" Seat)" = seat0 ] || continue
		[ "$(prop "$_s" Class)" = user ] || continue
		[ "$(prop "$_s" State)" = closing ] && continue
		echo "$_s"
	done | tail -n 1
}
greeter_session() {
	for _s in $(sessions); do
		[ "$(prop "$_s" Class)" = greeter ] || continue
		[ "$(prop "$_s" Seat)" = seat0 ] || continue
		echo "$_s"
	done | tail -n 1
}

diagnose() {
	echo "--- logind sessions"
	for _s in $(sessions); do
		printf '      %s ' "$_s"
		loginctl show-session "$_s" -p Name -p Class -p Type -p Service -p Active \
			-p State -p VTNr -p LockedHint 2>/dev/null | tr '\n' ' '
		echo
	done
	echo "--- foreground VT: $(cat /sys/class/tty/tty0/active 2>/dev/null)"
	echo "--- processes"
	pgrep -a 'sddm|kwin_wayland|plasmashell|ksplashqml|ksmserver|kscreenlocker|startplasma|getty|login' 2>/dev/null |
		sed 's/^/      /'
	echo "--- /var/log/sddm.log"
	tail -n 30 /var/log/sddm.log 2>/dev/null | sed 's/^/      /'
	if [ -n "$home" ] && [ -r "$home/.local/share/sddm/wayland-session.log" ]; then
		echo "--- $home/.local/share/sddm/wayland-session.log"
		tail -n 30 "$home/.local/share/sddm/wayland-session.log" | sed 's/^/      /'
	fi
}

# ── the states the checks wait for ──────────────────────────────────────
plasma_up() {
	owned org.kde.plasmashell || return 1
	pgrep -u "$uid" -x ksplashqml >/dev/null 2>&1 && return 1
	[ -n "$(user_session)" ] && [ "$(prop "$(user_session)" Active)" = yes ]
}
greeter_up() {
	_g=$(greeter_session)
	[ -n "$_g" ] && [ "$(prop "$_g" Active)" = yes ] || return 1
	pgrep -u sddm -f '^[^ ]*sddm-greeter' >/dev/null 2>&1 || return 1
	_u=$(user_session)
	[ -z "$_u" ] || [ "$(prop "$_u" Active)" != yes ]
}
plasma_gone() { ! pgrep -u "$uid" -x plasmashell >/dev/null 2>&1; }
saver_is() {
	sbus org.freedesktop.ScreenSaver /ScreenSaver org.freedesktop.ScreenSaver.GetActive |
		grep -q "$1"
}
hint_is() {
	_u=$(user_session)
	[ -n "$_u" ] && [ "$(prop "$_u" LockedHint)" = "$1" ]
}
locked_is() { saver_is "$1" && hint_is "$2"; }
active_vt() { cat /sys/class/tty/tty0/active 2>/dev/null; }
vt_is() { [ "$(active_vt)" = "tty$1" ]; }
counter() { cat /sys/power/suspend_stats/success 2>/dev/null || echo 0; }
counter_past() { [ "$(counter)" -gt "$1" ]; }

assert_session() {
	_u=$(user_session)
	if [ -z "$_u" ]; then
		bad "no logind session of $user on seat0"
		return
	fi
	for want in Active=yes Type=wayland Class=user; do
		if [ "$(prop "$_u" "${want%%=*}")" = "${want#*=}" ]; then
			ok "session $_u $want"
		else
			bad "session $_u is not $want ($(prop "$_u" "${want%%=*}"))"
		fi
	done
	if [ -n "$1" ]; then
		_svc=$(prop "$_u" Service)
		if [ "$_svc" = "$1" ]; then ok "session $_u opened by PAM service $1"
		else bad "session $_u opened by PAM service '$_svc', not $1"; fi
	fi
}

# The keyboard mode of a VT and the bytes waiting on its tty, through the
# ioctls the kernel answers them with: KDGKBMODE (K_OFF is 4) and FIONREAD.
# Opening the tty with O_NOCTTY reads nothing from it.
kbd_state() {
	python3 -I - "$1" <<'PY'
import fcntl, os, struct, sys
fd = os.open(sys.argv[1], os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
mode = struct.unpack("i", fcntl.ioctl(fd, 0x4B44, b"\0" * 4))[0]
pending = struct.unpack("i", fcntl.ioctl(fd, 0x541B, b"\0" * 4))[0]
print(mode, pending)
PY
}

case $check in
session)
	if until_true plasma_up; then
		ok "org.kde.plasmashell owned on $user's bus and KSplash gone"
	else
		bad "no Plasma session for $user within ${limit}s"
	fi
	assert_session "$arg"
	;;
greeter)
	if until_true greeter_up; then
		g=$(greeter_session)
		ok "greeter session $g (class greeter) is the active session on seat0"
		ok "$user has no active session"
	else
		bad "no active SDDM greeter on seat0 within ${limit}s"
	fi
	;;
logout)
	if ! plasma_up; then
		bad "$user has no Plasma session to log out of"
	else
		sbus org.kde.Shutdown /Shutdown org.kde.Shutdown.logout >/dev/null
		if until_true plasma_gone; then ok "plasmashell of $user exited"
		else bad "plasmashell of $user still runs after ${limit}s"; fi
		if until_true greeter_up; then
			ok "the greeter is the active session on seat0 again"
		else
			bad "no active SDDM greeter within ${limit}s of the logout"
		fi
	fi
	;;
locked | unlocked)
	want=true
	[ "$check" = unlocked ] && want=false
	hint=yes
	[ "$check" = unlocked ] && hint=no
	if until_true locked_is "$want" "$hint"; then
		ok "org.freedesktop.ScreenSaver GetActive is $want"
		ok "logind LockedHint is $hint"
	else
		if saver_is "$want"; then ok "org.freedesktop.ScreenSaver GetActive is $want"
		else bad "org.freedesktop.ScreenSaver GetActive is not $want within ${limit}s"; fi
		if hint_is "$hint"; then ok "logind LockedHint is $hint"
		else bad "logind LockedHint is not $hint within ${limit}s"; fi
	fi
	if [ "$check" = locked ]; then
		if pgrep -u "$uid" -x kscreenlocker_g >/dev/null 2>&1; then
			ok "kscreenlocker_greet runs as $user"
		else
			bad "kscreenlocker_greet is not running"
		fi
	fi
	;;
suspend)
	secs=${arg:-20}
	if ! grep -qw mem /sys/power/state 2>/dev/null; then
		bad "the kernel offers no suspend to RAM (/sys/power/state: $(cat /sys/power/state 2>/dev/null))"
	elif [ ! -r /sys/power/suspend_stats/success ]; then
		bad "no /sys/power/suspend_stats/success to count a completed suspend by"
	elif [ ! -w /sys/class/rtc/rtc0/wakealarm ]; then
		bad "no RTC wake alarm at /sys/class/rtc/rtc0/wakealarm"
	else
		before=$(counter)
		echo 0 > /sys/class/rtc/rtc0/wakealarm
		echo "+$secs" > /sys/class/rtc/rtc0/wakealarm
		info "suspends completed before: $before; RTC wakes in ${secs}s"
		loginctl suspend 2>&1 | sed 's/^/      /'
		if until_true counter_past "$before"; then
			ok "the machine suspended and resumed (suspend_stats/success $before -> $(counter))"
		else
			bad "no completed suspend within ${limit}s (suspend_stats/success stays $(counter))"
			dmesg 2>/dev/null | grep -E 'PM: |ACPI: PM' | tail -n 10 | sed 's/^/      /'
		fi
	fi
	;;
vt)
	[ -n "$arg" ] || { echo "flow-probe: vt needs a number" >&2; exit 64; }
	if until_true vt_is "$arg"; then ok "tty$arg is the foreground VT"
	else bad "the foreground VT is $(active_vt), not tty$arg"; fi
	u=$(user_session)
	if [ -z "$u" ]; then
		bad "no logind session of $user on seat0"
	else
		svt=$(prop "$u" VTNr)
		info "$user's session $u is on VT $svt"
		if [ "$svt" = "$arg" ]; then
			if [ "$(prop "$u" Active)" = yes ]; then ok "session $u is Active on its VT"
			else bad "session $u is not Active although its VT is in front"; fi
		else
			if [ "$(prop "$u" Active)" = no ]; then ok "session $u is not Active with tty$arg in front"
			else bad "session $u is still Active with tty$arg in front"; fi
		fi
	fi
	if [ "$arg" = 2 ]; then
		if pgrep -t tty2 -f 'getty|login' >/dev/null 2>&1; then ok "tty2 has its getty"
		else bad "nothing runs on tty2"; fi
	fi
	;;
clean)
	[ -n "$arg" ] || { echo "flow-probe: clean needs a path" >&2; exit 64; }
	rm -f "$arg"
	if [ -e "$arg" ]; then bad "$arg could not be removed"; else ok "$arg is absent"; fi
	;;
no-leak)
	[ -n "$arg" ] || { echo "flow-probe: no-leak needs a path" >&2; exit 64; }
	if [ -e "$arg" ]; then bad "$arg exists: a typed command ran"; else ok "$arg does not exist"; fi
	u=$(user_session)
	svt=""
	[ -n "$u" ] && svt=$(prop "$u" VTNr)
	if [ -z "$svt" ] || [ "$svt" = 0 ]; then
		bad "$user's session has no VT to look at"
	elif ! command -v python3 >/dev/null 2>&1; then
		bad "no python3 to ask the VT for its keyboard mode"
	else
		st=$(kbd_state "/dev/tty$svt" 2>&1) || st="error $st"
		mode=${st%% *}
		pending=${st#* }
		if [ "$mode" = 4 ]; then ok "tty$svt keyboard is K_OFF: keys reach the compositor alone"
		else bad "tty$svt keyboard mode is '$mode', not K_OFF (4)"; fi
		if [ "$pending" = 0 ]; then ok "tty$svt holds no unread input"
		else bad "tty$svt holds '$pending' unread byte(s)"; fi
	fi
	;;
*)
	echo "flow-probe: unknown check $check" >&2
	exit 64
	;;
esac

[ "$fails" -eq 0 ] || diagnose
exit "$fails"
