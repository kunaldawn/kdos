#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/02-greeter-password.sh — installed disk: greeter, password, Plasma
#
# Boots an overlay of the installed disk (KDOS_FLOW_DISK) with the ISO off.
# Asserts the greeter is the active session on seat0 with no session of the
# account, types the account's password into the greeter's focused field,
# then asserts a session opened by the sddm PAM service and that
# testing/plasma-probe.sh finds it whole.
# ---------------------------------
# shellcheck source=testing/flows/lib.sh
. "$(dirname "$0")/lib.sh"

flow_init 02-greeter-password
flow_disk
flow_run --disk "/kdos/$OVERLAY" --no-cdrom --until greeter \
	--root-script "$PROBE greeter" \
	--shot "$SHOT/01-greeter.ppm" \
	--type "$FLOW_PASSWORD" \
	--root-script "$PROBE session sddm" \
	--dismiss-picker \
	--shot "$SHOT/02-plasma.ppm" \
	--root-script testing/plasma-probe.sh
