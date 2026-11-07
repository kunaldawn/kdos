#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/07-no-tty-leak.sh — keys typed into Plasma reach no tty
#
# Boots the ISO into its autologin session and types `touch <file>` and
# Return with no window focused. The file is under /root, which the account
# cannot write: typing on the desktop opens KRunner, whose command runner
# runs the line as the account and succeeds on any path the account may
# write, so only a path it may not tells a leak from KRunner. Asserts the
# file does not exist, the session VT's keyboard is in K_OFF mode (KWin
# reads the input devices itself) and its tty holds no unread input.
# ---------------------------------
# shellcheck source=testing/flows/lib.sh
. "$(dirname "$0")/lib.sh"

leak=/root/kdos-flow-leak
flow_init 07-no-tty-leak
flow_run --dismiss-picker \
	--root-script "$PROBE clean $leak" \
	--type "touch $leak" \
	--shot "$SHOT/01-typed.ppm" \
	--keys esc \
	--root-script "$PROBE no-leak $leak"
