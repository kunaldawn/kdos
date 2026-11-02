#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/run.sh — run the login flows, all of them or those named
#
#   testing/flows/run.sh [FLOW...]      e.g. run.sh 04 06, or run.sh 04-lock-unlock
#
# Each flow is its own boot, one after another: two VMs at once would share
# the installed disk's base image and the rig's VNC port. Prints one verdict
# line per flow and exits with the number that failed.
# ---------------------------------
set -u
cd "$(dirname "$0")" || exit 1
want=$*
failed=0
ran=0
for f in [0-9][0-9]-*.sh; do
	if [ -n "$want" ]; then
		hit=0
		for w in $want; do
			case $f in "$w"*) hit=1 ;; esac
		done
		[ "$hit" = 1 ] || continue
	fi
	ran=$((ran + 1))
	sh "./$f" || failed=$((failed + 1))
done
[ "$ran" -gt 0 ] || { echo "flows: none matches '$want'" >&2; exit 64; }
echo "flows: $ran run, $failed failed"
exit "$failed"
