#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/04-lock-unlock.sh — Meta+L locks, the password unlocks
#
# Boots the ISO into its autologin session. Asserts the screen is not
# locked, presses Meta+L, and asserts org.freedesktop.ScreenSaver reports
# the lock, logind's LockedHint is set and kscreenlocker_greet runs. A Shift
# wakes the lock screen's prompt without typing into it, the password is
# typed, and the lock must be gone from both again.
# ---------------------------------
# shellcheck source=testing/flows/lib.sh
. "$(dirname "$0")/lib.sh"

flow_init 04-lock-unlock
flow_run --dismiss-picker \
	--root-script "$PROBE unlocked" \
	--chord meta+l \
	--root-script "$PROBE locked" \
	--shot "$SHOT/01-locked.ppm" \
	--keys shift \
	--type "$FLOW_PASSWORD" \
	--root-script "$PROBE unlocked" \
	--shot "$SHOT/02-unlocked.ppm"
