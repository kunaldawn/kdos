#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/plasma-probe.sh — is the Plasma session on a booted KDOS whole?
#
#   testing/vnc-shot.py --root-script 'testing/plasma-probe.sh [OPTIONS]'
#
#     --user NAME        the session's account (kdos)
#     --desk NAME        the desk the panels must match: kdos, dock or tiling.
#                        Default: the account's ~/.config/kdosrc [Desk] Layout,
#                        or kdos when it has none
#     --no-desk          skip the panel check
#     --phosphor         Phosphor mode is on: kdos_crt must be a loaded effect.
#                        Without it the effect must NOT be loaded
#     --window ERE       a window line must match ERE (repeatable). A line is
#                        class<TAB>desktop-file<TAB>geometry<TAB>state<TAB>
#                        sandbox-engine<TAB>sandbox-app-id<TAB>caption
#     --box ID           a window of box ID is mapped, tagged as the box, and
#                        the portal's accent colour inside the box is the
#                        session's
#
# RUNS IN THE GUEST, AS ROOT, on the serial console vnc-shot.py logs into, once
# its readiness gate has passed. Every check prints one line, `ok` or `FAIL`
# and what it looked at; the information a reader needs to act on a failure —
# every well-known name on the session bus, KWin's loaded effects, the
# renderer, logind's view of the session, the panels and the windows — is
# printed between them. The exit status is the number of failed checks, so 0
# is a whole session; vnc-shot.py fails the run on anything else.
#
# THE SESSION BUS IS ASKED AS THE USER. A session bus admits only the account
# that owns it — root's connection is dropped before Hello is answered — so
# every call on it runs under setpriv with the account's ids. setpriv opens no
# PAM session, so no check adds a login of its own to the session it looks at.
#
# WINDOWS COME FROM KWIN ITSELF. A KWin script lists workspace.windowList()
# and sends each window to a bus name nobody owns, io.kdos.RigProbe; the call
# fails, and dbus-daemon shows it to a monitor first, which is how the lines
# get out of the compositor without a service of the probe's own. The
# sandbox fields are the window properties KDOS's KWin patch adds, and read
# empty on a KWin without it.
# ---------------------------------
set -u

user=kdos
desk=""
checkdesk=1
phosphor=0
box=""
windows=""
while [ $# -gt 0 ]; do
	case $1 in
	--user) user=$2; shift ;;
	--desk) desk=$2; shift ;;
	--no-desk) checkdesk=0 ;;
	--phosphor) phosphor=1 ;;
	--window) windows="$windows$2
"; shift ;;
	--box) box=$2; shift ;;
	-h | --help) sed -n '9,28p' "$0"; exit 0 ;;
	*) echo "plasma-probe: unknown option $1" >&2; exit 64 ;;
	esac
	shift
done

checks=0
fails=0
ok() { checks=$((checks + 1)); echo "ok    $*"; }
bad() { checks=$((checks + 1)); fails=$((fails + 1)); echo "FAIL  $*"; }
info() { echo "      $*"; }
section() { echo; echo "== $*"; }

uid=$(id -u "$user" 2>/dev/null) || { echo "FAIL  no account $user"; exit 1; }
gid=$(id -g "$user")
as_user() { setpriv --reuid="$uid" --regid="$gid" --init-groups "$@"; }
home=$(awk -F: -v u="$user" '$1 == u { print $6 }' /etc/passwd)
run=/run/user/$uid
addr=unix:path=$run/bus
work=/run/kdos-rig/probe.$$
mkdir -p "$work"
chmod 0755 /run/kdos-rig "$work"
trap 'rm -rf "$work"' EXIT

# One call on the session bus; literal output, nothing on failure.
sbus() {
	_dest=$1 _path=$2 _method=$3
	shift 3
	as_user dbus-send --bus="$addr" --print-reply=literal --reply-timeout=10000 \
		--dest="$_dest" "$_path" "$_method" "$@" 2>/dev/null
}
ybus() {
	_dest=$1 _path=$2 _method=$3
	shift 3
	dbus-send --system --print-reply=literal --reply-timeout=10000 \
		--dest="$_dest" "$_path" "$_method" "$@" 2>/dev/null
}
# The names on a bus, one per line, from ListNames' literal array.
names() {
	tr -s ' \t' '\n' | sed -n '/^[A-Za-z_][A-Za-z0-9_.-]*\.[A-Za-z0-9_.-]*$/p' | sort -u
}

# A process of the account by name. A name past fifteen characters is past
# what the kernel keeps as the command name, so it is matched on the program
# path that starts the command line and then confirmed by the executable the
# process runs: a shell whose arguments mention the name is never taken for it.
running() {
	if [ ${#1} -le 15 ]; then
		pgrep -u "$uid" -x "$1" >/dev/null 2>&1
		return
	fi
	for _p in $(pgrep -u "$uid" -f "^[^ ]*/$1( |\$)" 2>/dev/null); do
		# Without CAP_SYS_PTRACE (a container, not the guest) the link is
		# unreadable, and the anchored command line is the evidence left.
		_e=$(readlink "/proc/$_p/exe" 2>/dev/null) || return 0
		case ${_e##*/} in
		"$1" | "$1 (deleted)") return 0 ;;
		esac
	done
	return 1
}

# ── processes ───────────────────────────────────────────────────────────
section "processes of $user ($uid)"
for p in kwin_wayland plasmashell ksmserver kded6 xdg-desktop-portal-kde \
	polkit-kde-authentication-agent-1; do
	if running "$p"; then ok "$p runs"; else bad "$p is not running"; fi
done

# ── session bus ─────────────────────────────────────────────────────────
section "session bus $run/bus"
if [ ! -S "$run/bus" ]; then
	bad "no session bus socket at $run/bus"
else
	owned=$(sbus org.freedesktop.DBus /org/freedesktop/DBus \
		org.freedesktop.DBus.ListNames | names)
	info "well-known names:"
	echo "$owned" | sed 's/^/        /'
	# A name answers when Peer.Ping comes back. An activatable name that is
	# not running yet is started by the ping, as by any client's first call,
	# and the line says so.
	for n in org.kde.KWin org.kde.plasmashell org.freedesktop.Notifications \
		org.kde.StatusNotifierWatcher org.freedesktop.portal.Desktop; do
		was=no
		echo "$owned" | grep -qx "$n" && was=yes
		if sbus "$n" / org.freedesktop.DBus.Peer.Ping >/dev/null; then
			if [ "$was" = yes ]; then ok "$n answers"
			else ok "$n answers (started by this probe's call)"; fi
		else
			bad "$n does not answer"
		fi
	done
	kd=$(echo "$owned" | grep -E '^(io|org)\.kdos\.' || true)
	if [ -n "$kd" ]; then
		for n in $kd; do
			if sbus "$n" / org.freedesktop.DBus.Peer.Ping >/dev/null; then
				ok "$n answers"
			else
				bad "$n is owned and does not answer"
			fi
		done
	else
		info "no io.kdos.* or org.kdos.* name on the session bus"
	fi
fi

# ── system bus ──────────────────────────────────────────────────────────
section "system bus"
if ybus org.freedesktop.login1 / org.freedesktop.DBus.Peer.Ping >/dev/null; then
	ok "org.freedesktop.login1 answers"
else
	bad "org.freedesktop.login1 does not answer"
fi
sysowned=$(ybus org.freedesktop.DBus /org/freedesktop/DBus \
	org.freedesktop.DBus.ListNames | names)
for n in $(echo "$sysowned" | grep -E '^(io|org)\.kdos\.' || true); do
	if ybus "$n" / org.freedesktop.DBus.Peer.Ping >/dev/null; then
		ok "$n answers"
	else
		bad "$n is owned and does not answer"
	fi
done
for n in org.freedesktop.hostname1 org.freedesktop.timedate1 \
	org.freedesktop.locale1 org.freedesktop.sysupdate1; do
	if echo "$sysowned" | grep -qx "$n"; then info "$n owned"; fi
done

# ── logind's session ────────────────────────────────────────────────────
# The session KWin runs in is logind's, through elogind: Active, a Wayland
# type and seat0 are what lets KWin take the DRM device and the inputs, and
# a KWin that could not take control says so in the session log.
section "loginctl"
sid=""
ppid_=$(pgrep -u "$uid" -x plasmashell 2>/dev/null | head -n 1)
[ -n "$ppid_" ] || ppid_=$(pgrep -u "$uid" -x kwin_wayland 2>/dev/null | head -n 1)
if [ -n "$ppid_" ] && [ -r "/proc/$ppid_/environ" ]; then
	sid=$({ tr '\0' '\n' < "/proc/$ppid_/environ"; } 2>/dev/null |
		sed -n 's/^XDG_SESSION_ID=//p' | head -n 1)
fi
if ! command -v loginctl >/dev/null 2>&1; then
	bad "no loginctl"
else
	if [ -z "$sid" ]; then
		sid=$(loginctl list-sessions --no-legend 2>/dev/null |
			awk -v u="$user" '$3 == u && $4 == "seat0" { print $1; exit }')
	fi
	if [ -z "$sid" ]; then
		bad "no logind session of $user on seat0"
		loginctl list-sessions --no-legend 2>/dev/null | sed 's/^/        /'
	else
		st=$(loginctl show-session "$sid" -p Active -p Type -p Seat -p Class -p VTNr -p State 2>/dev/null)
		echo "$st" | sed 's/^/      /'
		for want in Active=yes Type=wayland Seat=seat0; do
			if echo "$st" | grep -qx "$want"; then ok "session $sid $want"
			else bad "session $sid is not $want"; fi
		done
	fi
fi
slog="$home/.local/share/sddm/wayland-session.log"
if [ -r "$slog" ]; then
	if grep -q 'Failed to take control of' "$slog"; then
		bad "KWin could not take control of the session ($slog)"
		grep 'Failed to take control of' "$slog" | tail -n 3 | sed 's/^/        /'
	else
		ok "KWin took control of the session (no 'Failed to take control of' in $slog)"
	fi
else
	bad "no session log at $slog"
fi

# ── KWin ────────────────────────────────────────────────────────────────
section "kwin"
sbus org.kde.KWin /KWin org.kde.KWin.supportInformation | sed '1s/^   //' > "$work/support"
if [ ! -s "$work/support" ]; then
	bad "KWin answered no supportInformation"
else
	if grep -qx 'Compositing is active' "$work/support"; then
		ok "compositing is active"
	else
		bad "compositing is not active"
		sed -n '/^Compositing$/,/^$/p' "$work/support" | sed 's/^/        /'
	fi
	sed -n 's/^OpenGL renderer string: /renderer: /p' "$work/support" | sed 's/^/      /'
	awk '/^Loaded Effects:/ { on = 1; getline; next } on && /^$/ { exit } on { print }' \
		"$work/support" > "$work/effects"
	info "loaded effects: $(tr '\n' ' ' < "$work/effects")"
	if grep -qx kdos_crt "$work/effects"; then
		if [ "$phosphor" = 1 ]; then ok "kdos_crt is loaded (Phosphor mode)"
		else bad "kdos_crt is loaded with Phosphor mode off"; fi
	else
		if [ "$phosphor" = 1 ]; then bad "kdos_crt is not loaded (Phosphor mode is on)"
		else ok "kdos_crt is not loaded (Phosphor mode off)"; fi
	fi
fi

# ── the desk ────────────────────────────────────────────────────────────
# The panels the shell has, read from the shell itself with the same
# scripting API the desk's layout.js used to make them: each panel's edge and
# its widgets in order. A desk is matched by the widgets it must hold on the
# edge it puts them, not by the whole list, so a widget the user added does
# not fail it.
if [ "$checkdesk" = 1 ]; then
	if [ -z "$desk" ]; then
		desk=$(awk -F= '/^\[/ { g = $0 } g == "[Desk]" && $1 == "Layout" { print $2 }' \
			"$home/.config/kdosrc" 2>/dev/null | tail -n 1)
		[ -n "$desk" ] || desk=kdos
	fi
	section "desk ($desk)"
	js='var r = []; panels().forEach(function (p) { r.push(p.location + "|" + p.widgets().map(function (w) { return w.type; }).join(",")); }); print(r.join("\n"));'
	sbus org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript \
		"string:$js" | sed '1s/^   //' | grep '|' > "$work/panels"
	sed 's/^/      /' "$work/panels"
	# has EDGE WIDGET... : some panel on EDGE holds every WIDGET.
	has() {
		_edge=$1
		shift
		while IFS='|' read -r _loc _w; do
			[ "$_loc" = "$_edge" ] || continue
			_all=1
			for _x in "$@"; do
				case ",$_w," in *",org.kde.plasma.$_x,"*) ;; *) _all=0 ;; esac
			done
			[ "$_all" = 1 ] && return 0
		done < "$work/panels"
		return 1
	}
	edge() { grep -q "^$1|" "$work/panels"; }
	case $desk in
	kdos)
		if has bottom kickoff pager icontasks systemtray digitalclock; then
			ok "a bottom panel with Kickoff, the pager, the task manager, the tray and the clock"
		else bad "no bottom panel with the KDOS desk's widgets"; fi
		if edge top; then bad "a top panel on the KDOS desk"; else ok "no top panel"; fi
		;;
	dock)
		if has top kickoff appmenu systemtray digitalclock; then
			ok "a top bar with Kickoff, the global menu, the tray and the clock"
		else bad "no top bar with the Dock desk's widgets"; fi
		if has bottom icontasks; then ok "a bottom dock with the task manager"
		else bad "no bottom dock with the task manager"; fi
		;;
	tiling)
		if has top kickoff pager windowlist systemtray digitalclock; then
			ok "a top bar with Kickoff, the pager, the window list, the tray and the clock"
		else bad "no top bar with the Tiling desk's widgets"; fi
		if edge bottom; then bad "a bottom panel on the Tiling desk"; else ok "no bottom panel"; fi
		;;
	*) bad "no desk called $desk (kdos, dock, tiling)" ;;
	esac
fi

# ── windows, from a KWin script ─────────────────────────────────────────
section "windows"
token="probe$$"
script=$work/windows.js
cat > "$script" <<EOF
var ws = workspace.windowList();
for (var i = 0; i < ws.length; i++) {
    var w = ws[i];
    if (!w.normalWindow && !w.dialog)
        continue;
    var g = w.frameGeometry;
    var state = [w.minimized ? "minimized" : "", w.fullScreen ? "fullscreen" : "",
                 w.keepAbove ? "above" : ""].filter(function (s) { return s; }).join(",");
    var line = [w.resourceClass, w.desktopFileName,
                Math.round(g.x) + "," + Math.round(g.y) + " " + Math.round(g.width) + "x" + Math.round(g.height),
                state,
                w.sandboxEngine === undefined ? "" : w.sandboxEngine,
                w.sandboxAppId === undefined ? "" : w.sandboxAppId,
                w.caption.replace(/[\t\n"]/g, " ")].join("\t");
    callDBus("io.kdos.RigProbe", "/", "io.kdos.RigProbe", "Window", "$token", line);
}
callDBus("io.kdos.RigProbe", "/", "io.kdos.RigProbe", "Done", "$token");
EOF
chmod 0644 "$script"
setpriv --reuid="$uid" --regid="$gid" --init-groups dbus-monitor --address "$addr" "type='method_call',interface='io.kdos.RigProbe'" \
	> "$work/monitor" 2>&1 &
mon=$!
sleep 0.5
plugin="kdos-plasma-probe-$$"
sid_=$(sbus org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript \
	"string:$script" "string:$plugin" | awk '{ print $NF }')
case $sid_ in
'' | -* | *[!0-9]*)
	bad "KWin did not load the probe's script (loadScript answered '${sid_}')"
	;;
*)
	sbus org.kde.KWin "/Scripting/Script$sid_" org.kde.kwin.Script.run >/dev/null
	n=0
	while [ "$n" -lt 40 ] && ! grep -q 'member=Done' "$work/monitor"; do
		sleep 0.25
		n=$((n + 1))
	done
	sbus org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "string:$plugin" >/dev/null
	if grep -q 'member=Done' "$work/monitor"; then
		ok "KWin listed its windows"
	else
		bad "KWin's script never reported back"
	fi
	;;
esac
kill "$mon" 2>/dev/null
# The second string of each Window call is the line.
awk -v t="$token" '
	/member=Window/ { want = 2; next }
	want && /^ *string "/ {
		s = $0; sub(/^ *string "/, "", s); sub(/"$/, "", s)
		if (want == 2) { if (s != t) want = 0; else want = 1; next }
		print s; want = 0
	}' "$work/monitor" > "$work/windows"
if [ -s "$work/windows" ]; then
	sed 's/^/      /' "$work/windows"
else
	info "no normal windows"
fi
if [ -n "$windows" ]; then
	echo "$windows" | while IFS= read -r re; do
		[ -n "$re" ] || continue
		if grep -Eq -- "$re" "$work/windows"; then echo "ok    a window matches /$re/"
		else echo "FAIL  no window matches /$re/"; fi
	done > "$work/wanted"
	cat "$work/wanted"
	checks=$((checks + $(grep -c . "$work/wanted")))
	fails=$((fails + $(grep -c '^FAIL' "$work/wanted")))
fi

# ── a box ───────────────────────────────────────────────────────────────
# A boxed window carries the box's security-context app id, app.<box>, as its
# desktop file name, which is what the task manager matches its launcher by;
# KDOS's KWin also gives it the engine that confined it. The portal's accent
# is read inside the box over the box's proxied bus and must be the one the
# session's portal answers outside it.
if [ -n "$box" ]; then
	section "box $box"
	line=$(awk -F'\t' -v b="app.$box" '$2 == b || index($2, b ".") == 1 || $6 == b { print; exit }' \
		"$work/windows")
	if [ -n "$line" ]; then
		ok "a window of box $box is mapped"
		engine=$(echo "$line" | awk -F'\t' '{ print $5 }')
		case $engine in
		'') info "this KWin reports no sandbox engine" ;;
		io.kdos.appbox*) ok "the window is confined by $engine" ;;
		*) bad "the window's sandbox engine is '$engine', not KDOS's" ;;
		esac
	else
		bad "no window of box $box (desktop file app.$box)"
	fi
	outside=$(sbus org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop \
		org.freedesktop.portal.Settings.ReadOne string:org.freedesktop.appearance \
		string:accent-color |
		tr -s ' \n' ' ')
	# The box is entered the way a launcher enters it: as the account, from
	# its home, with the session's environment (vnc-shot.py's env.sh) and no
	# login of its own. The box name is an argument, never part of a script.
	# shellcheck disable=SC2016 # expanded by the inner shell
	inside=$(cd "$home" && as_user env -i HOME="$home" USER="$user" LOGNAME="$user" \
		PATH=/usr/local/bin:/usr/bin:/bin sh -c '. /run/kdos-rig/env.sh 2>/dev/null
			exec kdos-box run "$1" dbus-send --session --print-reply=literal \
				--dest=org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop \
				org.freedesktop.portal.Settings.ReadOne \
				string:org.freedesktop.appearance string:accent-color' probe "$box" 2> "$work/box.err" |
		tr -s ' \n' ' ')
	info "accent outside: $outside"
	info "accent inside:  $inside"
	case $inside in
	*double*)
		if [ "$inside" = "$outside" ]; then ok "the box reads the session's accent"
		else bad "the box reads a different accent"; fi
		;;
	*)
		bad "the box could not read the portal's accent"
		tail -n 5 "$work/box.err" | sed 's/^/        /'
		;;
	esac
fi

echo
echo "plasma-probe: $checks checks, $fails failed"
[ "$fails" -le 125 ] || fails=125
exit "$fails"
