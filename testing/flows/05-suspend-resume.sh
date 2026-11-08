#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/05-suspend-resume.sh — suspend, and resume onto the lock screen
#
# Boots the ISO into its autologin session, arms the emulated RTC to wake the
# machine, and suspends through logind, so PowerDevil's and the screen
# locker's sleep inhibitors run as they do for a closed lid. Asserts the
# kernel completed a suspend, that the session came back locked
# (kscreenlockerrc LockOnResume), that the password unlocks it, and that
# testing/plasma-probe.sh finds the session whole afterwards. It needs QEMU's
# S3 and the firmware's resume path; the probe fails naming the kernel's
# sleep states when they are absent.
# ---------------------------------
# shellcheck source=testing/flows/lib.sh
. "$(dirname "$0")/lib.sh"

flow_init 05-suspend-resume
flow_run --dismiss-picker \
	--root-script "$PROBE unlocked" \
	--root-script "$PROBE suspend 20" \
	--root-script "$PROBE locked" \
	--shot "$SHOT/01-resumed-locked.ppm" \
	--keys shift \
	--type "$FLOW_PASSWORD" \
	--root-script "$PROBE unlocked" \
	--shot "$SHOT/02-unlocked.ppm" \
	--root-script testing/plasma-probe.sh
