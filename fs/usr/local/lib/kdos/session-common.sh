#!/bin/sh
# session-common.sh — what BOTH sessions do, in one place.
#
# Sourced, never executed. The graphical session and the console session
# differ in which display they bring up and which portal backend they start;
# everything below is the same work, and every one of these blocks carries a
# trap that cost a debugging session to find. A second copy is a second place
# to lose one.
#
#   kdos_session_open      $BROWSER, which the login shell may not have set
#   kdos_session_runtime   XDG_RUNTIME_DIR, before anything uses it
#   kdos_session_keymap    the console keymap as XKB variables
#   kdos_session_boxes     the appbox warmup, and giving idle ones back
#   kdos_session_bus       one session bus per user, at a fixed path
#   kdos_session_audio     pipewire, once per user rather than per session
#   kdos_session_once      the login sound, the first-run card, the restore

# ONE ROAD TO A LINK, ON A PATH THAT READS NO PROFILE. /etc/profile.d sets
# $BROWSER for a login shell, and the `greet = yes` console session is exec'd
# from a program that clears the environment and runs this script directly — so
# the variable would exist on one supported login path and not the other, and
# `dbus-update-activation-environment BROWSER` pushes NOTHING for an unset name
# rather than failing. Only fills a gap: a person who exported their own has
# said what they want.
kdos_session_open() {
	BROWSER="${BROWSER:-xdg-open}"
	export BROWSER
}

kdos_session_runtime() {
	XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
	export XDG_RUNTIME_DIR
}

# THE CONSOLE SESSION NEEDS THIS TOO. libkkms reads the same XKB variables
# through xkbcommon that every Wayland client does, so leaving this in the
# graphical script gave the console US QWERTY on a machine whose owner does not
# type it.
kdos_session_keymap() {
	# The keyboard layout. The installer writes the CONSOLE keymap name to
	# /etc/keymap and kdos-getty loadkeys it on every tty — but nothing carried it
	# into the Wayland session, so a non-US user got US QWERTY in the desktop,
	# the lock-screen password prompt included. xkbcommon reads these variables in
	# every client and in kdos-comp itself; console names and XKB layout names are
	# different vocabularies, hence the table. Anything not listed falls back to
	# its first two letters, which is how most console maps are named anyway.
	if [ -r /etc/keymap ]; then
		_km=$(cat /etc/keymap 2>/dev/null)
		_layout= _variant=
		case "$_km" in
		"")        ;;
		us)        _layout=us ;;
		uk)        _layout=gb ;;
		de*)       _layout=de ;;
		fr*)       _layout=fr ;;
		es*)       _layout=es ;;
		it*)       _layout=it ;;
		br*)       _layout=br ;;
		ru*)       _layout=ru ;;
		# The console names whose first two letters are a DIFFERENT layout —
		# `la` is Lao, not Latin American, and there is no `sg`, `sl` or `cr`.
		sg*)       _layout=ch ;;
		slovene)   _layout=si ;;
		croat)     _layout=hr ;;
		la-latin1) _layout=latam ;;
		dvorak*)   _layout=us _variant=dvorak ;;
		*)         _layout=$(printf '%.2s' "$_km") ;;
		esac
		# A layout xkbcommon cannot compile does not fall back loudly: kdos-comp
		# logs the failure and sets XKB_DEFAULT_LAYOUT=us for the whole session,
		# lock prompt included — the exact bug this block exists to fix, arriving
		# silently. So a DERIVED name is only exported when xkeyboard-config
		# really carries it. The check is skipped where that data is absent,
		# because then nothing here can be verified either way.
		_xkb=/usr/share/X11/xkb/symbols
		if [ -n "$_layout" ] && [ -d "$_xkb" ] && [ ! -f "$_xkb/$_layout" ]; then
			echo "kdos-desktop: keymap '$_km' has no XKB layout '$_layout'" \
				"— leaving the default" >&2
			_layout=
		fi
		if [ -n "$_layout" ]; then
			export XKB_DEFAULT_LAYOUT="$_layout"
			[ -n "$_variant" ] && export XKB_DEFAULT_VARIANT="$_variant"
		fi
		unset _km _layout _variant _xkb
	fi
}

kdos_session_boxes() {
	# Pre-create and start the alien-app box while the desktop is coming up, so
	# the first launcher click doesn't pay for container init. nice 10, not 19:
	# at 19 the warmup loses every CPU slice to the starting desktop, so it was
	# still mid-init minutes later — and a launch that lands in that window has to
	# wait for it to finish.
	(nice -n 10 /usr/local/bin/kdos-appbox warmup >/dev/null 2>&1 &)

	# AND GIVE THE IDLE ONES BACK. `kdos-box gc` stops a box that has sat past its
	# profile's `autostop` with no window on the screen; it existed and nothing
	# called it, so a warmed box was a leak. Ten minutes, from the session rather
	# than from a root service, because the boxes are the user's and so is the
	# question the compositor is asked before any is stopped.
	(while sleep 600; do nice -n 10 /usr/local/bin/kdos-box gc >/dev/null 2>&1; done &)
}

kdos_session_bus() {
	# ONE session bus per user, at a fixed runtime path — not dbus-run-session.
	# dbus-run-session listens on unix:tmpdir=/tmp: a pathname socket in the
	# host's /tmp, which the appbox does NOT share — alien apps then see a
	# dangling DBUS_SESSION_BUS_ADDRESS: GApplication single-instance breaks
	# (every impatient re-click spawns another full instance), dconf/a11y probe
	# and stall, notifications go nowhere. $XDG_RUNTIME_DIR *is* shared with the
	# box, so this one address is valid on both sides. The daemon is reused
	# across session restarts; the address carries no guid on purpose — a later
	# session re-binding the socket would otherwise kill zbus clients with
	# "D-Bus handshake failed: Server GUID mismatch" (the session aborts).
	XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
	export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
	# flock-serialized: two sessions starting at once must not both decide the
	# daemon is down — the loser's rm -f would yank the winner's socket away.
	#
	# `7>&-` CLOSES THE LOCK FD FOR THE DAEMON, and without it the SECOND login of
	# a boot hangs forever with no output at all. dbus-daemon --fork inherits every
	# open descriptor, fd 7 among them; the subshell then exits and drops its own
	# reference, but the daemon holds the flock for as long as the session bus
	# lives — which is until logout. Every later kdos-desktop blocks on `flock 7`
	# before it has printed a single line, so the symptom is a tty that sits there:
	# no compositor, no error, nothing in a log. Measured in QEMU with three
	# `flock 7` processes queued behind one dbus-daemon.
	(
		flock 7
		if ! dbus-send --session --print-reply --dest=org.freedesktop.DBus \
			/ org.freedesktop.DBus.Peer.Ping >/dev/null 2>&1; then
			rm -f "$XDG_RUNTIME_DIR/bus"
			dbus-daemon --session --address="$DBUS_SESSION_BUS_ADDRESS" \
				--fork 7>&-
		fi
	) 7>"$XDG_RUNTIME_DIR/.kdos-bus.lock"
}

# THE AUDIO STACK, once per user and not once per session. pgrep-guarded
# because a session restart must not start a second pipewire: two of them
# fight over the same devices and the loser's clients get silence.
#
# Not Wayland's and not the console's — a login sound and a boxed application's
# audio are the same stack on either desktop.
kdos_session_audio() {
	if command -v pipewire >/dev/null 2>&1 && ! pgrep -f '^pipewire$' >/dev/null 2>&1; then
		pipewire >/dev/null 2>&1 &
		sleep 0.3
		command -v pipewire-media-session >/dev/null 2>&1 && \
			pipewire-media-session >/dev/null 2>&1 &
		command -v pipewire-pulse >/dev/null 2>&1 && \
			pipewire-pulse >/dev/null 2>&1 &
	fi
}

# Everything a session does ONCE, after its display exists.
#
#   kdos_session_once <command...>
#
# The command is the caller's readiness test and must BLOCK until the display
# is up, exporting whatever the children need to reach it, and return non-zero
# if it never comes. What "up" means differs — a Wayland socket for one session,
# a session socket for the other — and only the caller knows.
#
# All of it is one backgrounded subshell, because none of it may delay the
# display by a millisecond: the session starts whether or not any of the three
# is installed or wanted.
kdos_session_once() {
	(
		"$@" || exit 0

		_cfg="${XDG_CONFIG_HOME:-$HOME/.config}"

		# The login chord. Fire and forget, and silent about failure: an
		# image with no kdos-sfx, or a machine with no card, must not
		# print anything at every login.
		command -v kdos-sfx >/dev/null 2>&1 && kdos-sfx login >/dev/null 2>&1 &

		# The keybind card, once. kdos-keys writes the marker itself when
		# the user has seen it — this only decides whether to open it, so a
		# user who deletes the marker gets the welcome back.
		if [ ! -e "$_cfg/kdos/first-run" ] && \
		   command -v kdos-keys >/dev/null 2>&1; then
			kdos-keys --first-run >/dev/null 2>&1 &
		fi

		# Session restore, opt-in: ~/.config/kdos/session-restore has to
		# exist, because relaunching half a dozen containerised
		# applications at login is a decision, not a default. Only the
		# `app` lines are relaunched — a native program costs no container
		# start, and where every window comes back is the desktop's window
		# memory rather than this list. Staggered, because six
		# `kdos-appbox run` at once contend for the same box the login
		# warmup is still building.
		# THE PER-USER TIMERS, and they die with the session.
		#
		# `ksvc supervise` writes its pidfile into /run, which an
		# ordinary user cannot, so the system table's supervisor is not
		# available here — and it should not be: a job that writes into
		# $HOME has no business outliving the login that started it,
		# and a supervised one would go on firing on a machine the
		# person has walked away from. The loops below are this
		# session's own children and die with it, and their pids are
		# in the session's runtime directory rather than in /run.
		#
		# The table is parsed the same way /etc/kdos/timers.d is —
		# `NAME TIMESPEC... -- COMMAND...`, an argument vector and not
		# a shell line — because two parses of one format is one of
		# them being wrong the day a field is added.
		#
		# AND THEY REPEAT, WHICH TOOK A PIDFILE TO MAKE POSSIBLE.
		# `snooze` waits for its slot, runs the command ONCE and exits,
		# so a bare background job is one run per login and not a
		# schedule — a reminder set for every Monday fired on the
		# Monday somebody happened to log in. The loop that repeats it
		# has to be FINDABLE, or a start script that re-execs itself
		# adds a second set of loops on top of the first: the pid of
		# each goes in the session's own runtime directory, which is
		# cleared when the login ends, and the next start stops what
		# the last one left before it starts anything.
		#
		_tpid="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/kdos/timers.pid"
		mkdir -p "${_tpid%/*}" 2>/dev/null
		if [ -r "$_tpid" ]; then
			while read -r _p; do
				case "$_p" in
					''|*[!0-9]*) continue ;;
				esac
				# THE CHILD FIRST. Killing the loop alone
				# leaves the `snooze` it is waiting on
				# running, reparented and unfindable.
				pkill -P "$_p" 2>/dev/null
				kill "$_p" 2>/dev/null
			done < "$_tpid"
		fi
		: > "$_tpid" 2>/dev/null

		_td="$_cfg/kdos/timers.d"
		if [ -d "$_td" ] && command -v snooze >/dev/null 2>&1; then
			for _tf in "$_td"/*.timer; do
				[ -e "$_tf" ] || continue
				while IFS= read -r _row || [ -n "$_row" ]; do
					case "$_row" in
						''|'#'*) continue ;;
					esac
					# NOGLOB AROUND THE SPLIT, for the
					# reason /etc/init.d/18_timers.sh gives:
					# `*` is snooze's own syntax for
					# "every", and an unguarded split
					# expands it against the current
					# directory.
					set -f
					set -- $_row
					set +f
					shift	# the name; it labels only
					_spec=""
					_seen=0
					while [ $# -gt 0 ]; do
						if [ "$1" = "--" ]; then
							_seen=1
							shift
							break
						fi
						_spec="$_spec $1"
						shift
					done
					[ "$_seen" = 1 ] && [ $# -gt 0 ] || continue
					command -v "$1" >/dev/null 2>&1 || continue
					# THE SPEC IS CHECKED FIRST. `snooze -n`
					# prints the next five times and exits
					# non-zero on a pattern it will not
					# accept, so a row it would refuse is
					# reported by nothing otherwise.
					# shellcheck disable=SC2086
					snooze -n $_spec >/dev/null 2>&1 ||
						continue
					#
					# THE LOOP IS WHAT REPEATS IT, and it
					# is the same shape `supervise` gives
					# the system table: `snooze` waits for
					# its slot, runs the command once and
					# exits, so something has to start it
					# again. The floor under the loop is
					# what stops a command that fails
					# instantly from spinning it at the
					# speed of `fork` — one second is far
					# below any schedule and far above a
					# spin.
					# shellcheck disable=SC2086
					(
						while :; do
							_t0=$(date +%s)
							snooze $_spec "$@" \
								>/dev/null 2>&1
							[ $(( $(date +%s) - \
							      _t0 )) -ge 1 ] ||
								sleep 1
						done
					) >/dev/null 2>&1 &
					echo $! >> "$_tpid" 2>/dev/null
				done < "$_tf"
			done
		fi

		_sess="${XDG_STATE_HOME:-$HOME/.local/state}/kdos/session"
		if [ -e "$_cfg/kdos/session-restore" ] && [ -r "$_sess" ] && \
		   command -v kdos-appbox >/dev/null 2>&1; then
			while read -r _kind _name; do
				[ "$_kind" = app ] || continue
				[ -n "$_name" ] || continue
				# kdos-appbox waits out an in-flight warmup itself.
				kdos-appbox run "$_name" >/dev/null 2>&1 &
				sleep 2
			done < "$_sess"
		fi
	) &
}
