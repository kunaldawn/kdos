#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/05-suspend-resume.sh — suspend, as the rig's display allows it
#
# The rig's display is virtio-vga, and virtio-gpu does not survive a resume:
# 40_elogind.sh turns suspend off whenever a virtio-gpu device is bound. So
# the flow boots the ISO into its autologin session and asserts the refusal:
# logind's CanSuspend answers no, `loginctl suspend` does not suspend, and the
# session is still up, unlocked, lit and whole (testing/plasma-probe.sh)
# after it.
# The resume path itself, the probe's `suspend` check with the RTC armed and
# the session coming back locked, needs a display that resumes, which no rig
# configuration has.
# ---------------------------------
# shellcheck source=testing/flows/lib.sh
. "$(dirname "$0")/lib.sh"

flow_init 05-suspend-resume
FLOW_LIT="01-after-refusal.ppm"
flow_run --dismiss-picker \
	--root-script "$PROBE unlocked" \
	--root-script "$PROBE no-suspend" \
	--root-script "$PROBE session sddm-autologin" \
	--root-script "$PROBE unlocked" \
	--shot "$SHOT/01-after-refusal.ppm" \
	--root-script testing/plasma-probe.sh
