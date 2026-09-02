#!/bin/sh
# session-common.sh — what BOTH sessions do, in one place.
#
# Sourced, never executed. The graphical session and the console session
# differ in which display they bring up and which portal backend they start;
# everything below is the same work, and every one of these blocks carries a
# trap that cost a debugging session to find. A second copy is a second place
# to lose one.
#
#   kdos_session_runtime   XDG_RUNTIME_DIR, before anything uses it
#   kdos_session_keymap    the console keymap as XKB variables
#   kdos_session_boxes     the appbox warmup, and giving idle ones back
#   kdos_session_bus       one session bus per user, at a fixed path
#   kdos_session_audio     pipewire, once per user rather than per session
#   kdos_session_once      the login sound, the first-run card, the restore

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
