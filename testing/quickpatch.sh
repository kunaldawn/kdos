#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   Put freshly built files onto a booted ISO, and restart what reads them.
#
# RUNS IN THE GUEST, as root, from `testing/quick.sh`'s --root-script, once
# vnc-shot.py's gate has seen the Plasma session up. The tar arrives on a raw
# virtio disk that vnc-shot.py attached with --data-disk: no partition table
# and no filesystem, so it is read with `dd` straight off the device.
#
# THIS EXISTS BECAUSE THE ISO STEP IS SIX MINUTES AND A PLUGIN IS TWO HUNDRED
# KILOBYTES. Rebuilding the image to look at a changed plasmoid spends nearly
# all of an iteration repacking an image that did not change. The live
# medium's root is an overlay in RAM, so untarring over it is both allowed and
# thrown away at the next boot — which is the property that makes this safe
# rather than a way to corrupt a test image.
#
# WHAT IS RESTARTED FOLLOWS FROM WHAT THE TAR HOLDS, one rule per kind of
# component, and the machine is never rebooted, because the patch lives only
# in the RAM overlay:
#
#   KWin itself, a KWin plugin, a decoration,   org.kde.KWin replace: KWin
#   a compiled effect, a QML effect or script   exits 133 and its wrapper starts
#                                               the new binary; the session's
#                                               windows reconnect to it
#   a JavaScript effect (kwin/effects/<id>)     unloadEffect, loadEffect — only
#                                               when it was loaded
#   a JavaScript KWin script                    unloadScript, loadScript, run —
#   (kwin/scripts/<id>)                         only when it was loaded
#   kded6, a kded module (kf6/kded/<name>.so)   kded6 --replace, with the old
#                                               kded6's environment
#   a KRunner plugin                            krunner quits; the next query
#                                               starts it again
#   plasmashell, a plasmoid, a Plasma package,  plasmashell quits and is started
#   libplasma, an org.kde QML module            again with its old environment
#   a KCM, a Discover backend, any program      its running instances are
#                                               ended; the next launch is the
#                                               new code
#
# ONLY JAVASCRIPT RELOADS INSIDE A RUNNING PROCESS. A plugin library stays
# mapped once Qt has loaded it, and loading the same path again hands back the
# old code; KWin's QML effects and scripts share one QML engine, whose type
# cache answers a reload of the same file with the type it compiled first. A
# reload of either would photograph the old code and call it the new, so both
# take a new process. A JavaScript effect or script gets an engine of its own
# per load and does not.
#
# WHAT IT CANNOT CARRY: an SDDM theme (no greeter runs in the session it
# patches; log out to see it), anything under `fs/`, and anything that owns a
# D-Bus name on the session bus — a restarted owner races its old self for the
# name and clients hold the old one, so a daemon or a notification server is
# checked on a real boot. RESTART=none leaves the session alone, for a program
# the run spawns fresh anyway.
# ---------------------------------
set -u

# Substituted by quick.sh: the guest inherits nothing from the host, so
# the choices are baked into the copy that is sent. They are declared
# HERE, above every reader — `set -u` makes a use before this point an
# abort, which ends the script silently in the middle of a patch.
KEEP=0
SKEL=0
RESTART=auto
USER_NAME=kdos

log() { echo "quickpatch: $*"; }

[ "$KEEP" = 1 ] && RESTART=none

uid=$(id -u "$USER_NAME" 2>/dev/null) || { log "no account $USER_NAME"; exit 1; }
gid=$(id -g "$USER_NAME")
home=$(awk -F: -v u="$USER_NAME" '$1 == u { print $6 }' /etc/passwd)
addr=unix:path=/run/user/$uid/bus
slog=$home/.local/share/sddm/wayland-session.log
work=/run/kdos-rig/quickpatch
mkdir -p "$work"

# The session bus admits only its own account, so every call on it is made
# as that account; setpriv opens no PAM session doing it.
as_user() { setpriv --reuid="$uid" --regid="$gid" --init-groups "$@"; }
call() {
	as_user dbus-send --bus="$addr" --print-reply=literal --reply-timeout=15000 "$@" 2>&1
}
owned() {
	call --dest=org.freedesktop.DBus /org/freedesktop/DBus \
		org.freedesktop.DBus.NameHasOwner "string:$1" | grep -q true
}

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
# which is why this needs no length and no truncate. The names it unpacks are
# what decides the restarts below.
if ! dd if="$disk" bs=1M 2>/dev/null |
	tar xvf - -C / > "$work/files" 2> "$work/err"; then
	log "untar failed:"
	cat "$work/err"
	exit 1
fi
sed -i 's|^\./||' "$work/files"
log "unpacked $(grep -c . "$work/files") files"

# ── THE LIVE USER'S COPY OF A SKEL FILE ─────────────────────────────────
#
# `/etc/skel` seeds a NEW account and the desktop user's home was seeded at
# install time, so a file patched in skel changes nothing for the person
# logged in. When the tar carried one, copy it over — this is a test harness on
# a medium whose root is thrown away at the next boot, and the alternative is a
# config change that cannot be tested at all without a full build.
if [ -d /etc/skel/.config ] && [ "$SKEL" = 1 ]; then
	cp -r /etc/skel/.config/. "$home/.config/" 2>/dev/null
	chown -R "$uid:$gid" "$home/.config" 2>/dev/null
	log "skel copied over the live user's config"
fi

if [ "$RESTART" = none ]; then
	log "session left alone — the new files are read when something starts"
	sync
	exit 0
fi

# ── what the tar holds ──────────────────────────────────────────────────
has() { grep -Eq "$1" "$work/files"; }
ids() { sed -En "s#$1#\\1#p" "$work/files" | sort -u; }
# The API a KWin package of the tar is written against, from its metadata.json:
# javascript or declarativescript.
api_of() {
	sed -n 's/.*"X-Plasma-API"[^"]*"\([^"]*\)".*/\1/p' "$1/metadata.json" 2>/dev/null | head -n 1
}
effect_ids() { ids '^usr/share/kwin/effects/([^/]+)/.*'; }
script_ids() { ids '^usr/share/kwin/scripts/([^/]+)/.*'; }
# True when the tar holds a QML effect or a QML KWin script, which only a new
# KWin reads afresh.
kwin_qml() {
	for _e in $(effect_ids); do
		[ "$(api_of "/usr/share/kwin/effects/$_e")" = declarativescript ] && return 0
	done
	for _s in $(script_ids); do
		[ "$(api_of "/usr/share/kwin/scripts/$_s")" = declarativescript ] && return 0
	done
	return 1
}
# Start the words of $2 — a fixed command, never data — as the account, with
# exactly the environment in file $1, detached from this script and writing to
# the session log. xargs hands the NUL-separated environment over as
# arguments, so no value is ever read by a shell.
respawn() {
	QP_CMD=$2
	export QP_CMD
	# shellcheck disable=SC2016
	xargs -0 sh -c 'u=$1 g=$2; shift 2
		exec setpriv --reuid="$u" --regid="$g" --init-groups \
			env -i "$@" setsid -f $QP_CMD' quickpatch "$uid" "$gid" \
		< "$1" >> "$slog" 2>&1
}
# Wait up to 30 s for a process $1 of the account other than pid $2 to run
# while bus name $3 is owned; prints its pid.
came_back() {
	_i=0
	while [ $_i -lt 120 ]; do
		_now=$(pgrep -u "$uid" -x "$1" | head -n 1)
		if [ -n "$_now" ] && [ "$_now" != "$2" ] && owned "$3"; then
			echo "$_now"
			return 0
		fi
		sleep 0.25
		_i=$((_i + 1))
	done
	return 1
}
want() {
	case ",$RESTART," in
	*",$1,"*) return 0 ;;
	esac
	[ "$RESTART" = auto ] || return 1
	case $1 in
	kwin) has '^usr/bin/kwin_wayland|^usr/lib/libkwin\.so|^usr/lib/qt6/plugins/kwin/plugins/|^usr/lib/qt6/plugins/kwin/effects/plugins/|^usr/lib/qt6/plugins/org\.kde\.kdecoration3/|^usr/lib/libkdecorations3|^usr/share/kwin/decorations/' || kwin_qml ;;
	plasmashell) has '^usr/bin/plasmashell$|^usr/lib/libplasma|^usr/lib/qt6/plugins/plasma/(applets|containmentactions)/|^usr/share/plasma/(plasmoids|shells|look-and-feel|desktoptheme|layout-templates|wallpapers)/|^usr/lib/qt6/qml/org/kde/' ;;
	kded) has '^usr/bin/kded6$|^usr/lib/qt6/plugins/kf6/kded/' ;;
	krunner) has '^usr/lib/qt6/plugins/kf6/krunner/|^usr/share/krunner/' ;;
	*) return 1 ;;
	esac
}

fails=0

# ── KWin ────────────────────────────────────────────────────────────────
#
# THE WAIT IS FOR A DIFFERENT PID AND THE NAME, not for the socket: the
# wrapper keeps the Wayland socket across the restart, so "the socket is
# there" is true throughout and says nothing about which KWin answers it.
if want kwin; then
	old=$(pgrep -u "$uid" -x kwin_wayland | head -n 1)
	log "KWin: replacing ${old:-(none running)}"
	call --dest=org.kde.KWin /KWin org.kde.KWin.replace >/dev/null
	if now=$(came_back kwin_wayland "${old:-}" org.kde.KWin); then
		log "KWin: $now answers"
	else
		log "KWin did not come back"
		fails=$((fails + 1))
	fi
else
	# A JavaScript effect or script reloads inside the running KWin, and only
	# when it was loaded: loading one the session had turned off would
	# photograph a desktop nobody runs. A compiled or QML one reaches this
	# branch only when KDOS_QUICK_RESTART left KWin out, and is then left as
	# it was.
	for e in $(ids '^usr/lib/qt6/plugins/kwin/effects/plugins/([^/]+)\.so$'); do
		log "effect $e is compiled — its new code runs once KWin is replaced (KDOS_QUICK_RESTART=kwin)"
	done
	for e in $(effect_ids); do
		if [ "$(api_of "/usr/share/kwin/effects/$e")" = declarativescript ]; then
			log "effect $e is QML — its new code runs once KWin is replaced (KDOS_QUICK_RESTART=kwin)"
			continue
		fi
		if call --dest=org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded \
			"string:$e" | grep -q true; then
			call --dest=org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect "string:$e" >/dev/null
			if call --dest=org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect \
				"string:$e" | grep -q true; then
				log "effect $e reloaded"
			else
				log "effect $e did not load again"
				fails=$((fails + 1))
			fi
		else
			log "effect $e is not loaded in this session — left off"
		fi
	done
	for s in $(script_ids); do
		dir=/usr/share/kwin/scripts/$s
		if [ "$(api_of "$dir")" = declarativescript ]; then
			log "script $s is QML — its new code runs once KWin is replaced (KDOS_QUICK_RESTART=kwin)"
			continue
		fi
		if ! call --dest=org.kde.KWin /Scripting org.kde.kwin.Scripting.isScriptLoaded \
			"string:$s" | grep -q true; then
			log "script $s is not loaded in this session — left off"
			continue
		fi
		main=$(sed -n 's/.*"X-Plasma-MainScript"[^"]*"\([^"]*\)".*/\1/p' "$dir/metadata.json" 2>/dev/null | head -n 1)
		main=$dir/contents/${main:-code/main.js}
		call --dest=org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "string:$s" >/dev/null
		# unloadScript deletes the script later, from KWin's event loop; a
		# load before that finds the name still taken and answers -1.
		sleep 0.5
		id=$(call --dest=org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript \
			"string:$main" "string:$s" | awk '{ print $NF }')
		case $id in
		'' | -* | *[!0-9]*)
			log "script $s did not load again (loadScript answered '$id')"
			fails=$((fails + 1))
			;;
		*)
			call --dest=org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run >/dev/null
			log "script $s reloaded"
			;;
		esac
	done
fi

# ── kded modules ────────────────────────────────────────────────────────
#
# A NEW kded6, with the environment the running one has: --replace asks the
# old one to quit through its KDBusService name, and the new one loads every
# autoload module, the patched ones among them, from disk.
if want kded; then
	old=$(pgrep -u "$uid" -x kded6 | head -n 1)
	if [ -z "$old" ]; then
		log "kded6 is not running — nothing to restart"
		fails=$((fails + 1))
	else
		cp "/proc/$old/environ" "$work/kded.environ"
		log "kded6: replacing $old for $(ids '^usr/lib/qt6/plugins/kf6/kded/([^/]+)\.so$' | tr '\n' ' ')"
		respawn "$work/kded.environ" "kded6 --replace"
		if now=$(came_back kded6 "$old" org.kde.kded6); then
			log "kded6: $now answers"
		else
			log "kded6 did not come back"
			fails=$((fails + 1))
		fi
	fi
fi

# ── KRunner ─────────────────────────────────────────────────────────────
if want krunner && owned org.kde.krunner; then
	call --dest=org.kde.krunner /MainApplication org.qtproject.Qt.QCoreApplication.quit >/dev/null
	log "krunner quit; the next query starts the new one"
fi

# ── plasmashell ─────────────────────────────────────────────────────────
#
# STARTED AGAIN WITH THE ENVIRONMENT IT HAD. plasma_session started the first
# one with the session's environment, and nothing else holds a copy, so it is
# read from the old process before that process is asked to quit; the new one
# is started from it as the account, detached, writing to the session log.
if want plasmashell; then
	old=$(pgrep -u "$uid" -x plasmashell | head -n 1)
	if [ -z "$old" ]; then
		log "plasmashell is not running — nothing to restart"
		fails=$((fails + 1))
	else
		cp "/proc/$old/environ" "$work/environ"
		log "plasmashell: quitting $old"
		call --dest=org.kde.plasmashell /MainApplication \
			org.qtproject.Qt.QCoreApplication.quit >/dev/null
		i=0
		while kill -0 "$old" 2>/dev/null && [ $i -lt 40 ]; do
			sleep 0.25
			i=$((i + 1))
		done
		kill -0 "$old" 2>/dev/null && kill -TERM "$old"
		sleep 0.5
		respawn "$work/environ" plasmashell
		if now=$(came_back plasmashell "$old" org.kde.plasmashell); then
			# The name is taken early in main(); the panels follow.
			sleep 3
			log "plasmashell: $now up"
		else
			log "plasmashell did not come back"
			fails=$((fails + 1))
		fi
	fi
fi

# ── programs the tar replaced ───────────────────────────────────────────
#
# A program the session runs from a replaced binary, or a host of replaced
# plugins — System Settings and kcmshell6 for a KCM, Discover for a backend —
# is ended, and the run's next step starts the new code. KWin, kded6 and
# plasmashell were handled above. A program owning a D-Bus name is ended too and comes
# back only when something starts it, which is this loop's stated limit.
# The kernel keeps fifteen characters of a command name, so the match on it
# is confirmed by the executable each process runs.
hosts=""
# A program's file name holds no blank, so the list splits on words.
# shellcheck disable=SC2013
for f in $(grep -E '^usr/(local/)?bin/[^/]+$' "$work/files"); do
	hosts="$hosts ${f##*/}"
done
has '^usr/lib/qt6/plugins/plasma/kcms/' && hosts="$hosts systemsettings kcmshell6"
has '^usr/lib/qt6/plugins/discover/|^usr/lib/plasma-discover/' && hosts="$hosts plasma-discover"
for b in $hosts; do
	case $b in kwin_wayland | kwin_wayland_wrapper | plasmashell | kded6) continue ;; esac
	for p in $(pgrep -u "$uid" -x "$(printf %.15s "$b")" 2>/dev/null); do
		e=$(readlink "/proc/$p/exe" 2>/dev/null)
		case ${e##*/} in
		"$b" | "$b (deleted)") kill -TERM "$p" 2>/dev/null && log "ended $b ($p)" ;;
		esac
	done
done

if [ -s "$work/err" ]; then
	log "tar said:"
	cat "$work/err"
fi
if [ "$fails" -gt 0 ]; then
	log "$fails restart(s) failed — the session log says:"
	tail -n 30 "$slog" 2>/dev/null
fi
sync
exit "$fails"
