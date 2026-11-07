#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/06-vt-switch.sh — Ctrl+Alt+F2 reaches tty2, and back
#
# Boots the ISO into its autologin session. Ctrl+Alt+F2 must bring tty2 to
# the front with its getty while logind marks the Plasma session inactive;
# Ctrl+Alt+F<n> for the session's own VT (KDOS_FLOW_SESSION_VT, 1) must make
# it the foreground VT and the session Active again. The probe prints the VT
# logind records for the session, which is the number to set when it is not
# the one pressed.
# ---------------------------------
# shellcheck source=testing/flows/lib.sh
. "$(dirname "$0")/lib.sh"

flow_init 06-vt-switch
flow_run --dismiss-picker \
	--root-script "$PROBE vt $FLOW_VT" \
	--chord ctrl+alt+f2 \
	--root-script "$PROBE vt 2" \
	--shot "$SHOT/01-tty2.ppm" \
	--chord "ctrl+alt+f$FLOW_VT" \
	--root-script "$PROBE vt $FLOW_VT" \
	--shot "$SHOT/02-session.ppm"
