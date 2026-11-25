#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   The speech server a session's screen reader started, ended with it.
#
# Orca, KMouth and spd-say start Speech Dispatcher on first use (`--spawn`):
# it detaches, outlives every client of the session, and listens in
# $XDG_RUNTIME_DIR/speech-dispatcher, which turnstiled removes at the user's
# last logout. Left running, it holds its two sd_espeak-ng modules past the
# logout, and the next login, finding no socket, spawns a second server
# beside it; every screen-reader login would leave three processes behind.
#
# The server is named by its own pid file and sent SIGTERM, on which it stops
# its modules and removes the file. Never a pattern kill: the server's comm is
# cut to 15 characters ("speech-dispatch") and a pattern matches its own
# command line. The PID is checked against that comm first, so a stale file
# naming a reused PID ends nothing. While another login of the same user is
# open (a console, a second graphical login), the runtime directory stays and
# that login may be speaking through the server, so it is left running. The
# session turnstiled opens for the user's service manager (PAM service
# "turnstiled") is listed beside every login and is not counted: counting it
# would leave the server running at every logout.
#
# Bounded, as every shutdown script must be: a logout waits for it.
# ---------------------------------

pidfile=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/speech-dispatcher/pid/speech-dispatcher.pid
[ -r "$pidfile" ] || exit 0

if command -v loginctl >/dev/null 2>&1; then
	logins=0
	for s in $(timeout 5 loginctl show-user "$(id -u)" -p Sessions --value 2>/dev/null); do
		svc=$(timeout 5 loginctl show-session "$s" -p Service --value 2>/dev/null)
		[ "$svc" = turnstiled ] && continue
		logins=$((logins + 1))
	done
	[ "$logins" -gt 1 ] && exit 0
fi

read -r pid <"$pidfile" || exit 0
case $pid in '' | *[!0-9]*) exit 0 ;; esac
read -r comm <"/proc/$pid/comm" 2>/dev/null || exit 0
[ "$comm" = speech-dispatch ] || exit 0
echo "kdos: session end: speech-dispatcher $pid" >&2
kill -TERM "$pid" 2>/dev/null
exit 0
