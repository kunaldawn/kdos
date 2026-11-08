#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/03-logout-greeter.sh — logging out returns to the greeter
#
# The installed disk again, because the live medium's autologin carries
# Relogin=true and logs straight back in. Logs in at the greeter, then asks
# ksmserver to log out with the call the Log Out button makes
# (org.kde.Shutdown logout), and asserts that plasmashell exits, the
# account's session is no longer active and the greeter is the active
# session on seat0.
# ---------------------------------
# shellcheck source=testing/flows/lib.sh
. "$(dirname "$0")/lib.sh"

flow_init 03-logout-greeter
flow_disk
flow_run --disk "/kdos/$OVERLAY" --no-cdrom --until greeter \
	--root-script "$PROBE greeter" \
	--type "$FLOW_PASSWORD" \
	--root-script "$PROBE session sddm" \
	--dismiss-picker \
	--shot "$SHOT/01-plasma.ppm" \
	--root-script "$PROBE logout" \
	--shot "$SHOT/02-greeter.ppm"
