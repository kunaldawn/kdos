#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/01-live-autologin.sh — the live medium logs straight in
#
# Boots the ISO. SDDM's autologin opens the session with no greeter, KSplash
# runs to its last stage and Plasma comes up. Asserts that the session was
# opened by the sddm-autologin PAM service, is Active, Wayland and of class
# user on seat0, and that testing/plasma-probe.sh finds it whole.
# ---------------------------------
# shellcheck source=testing/flows/lib.sh
. "$(dirname "$0")/lib.sh"

flow_init 01-live-autologin
flow_run --dismiss-picker \
	--root-script "$PROBE session sddm-autologin" \
	--shot "$SHOT/01-plasma.ppm" \
	--root-script testing/plasma-probe.sh
