#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   The desktop, driven the way a person drives it.
#
#   testing/usability.sh              # the ISO, 1280x800
#   testing/usability.sh 1920x1080    # another size, so another panel length
#
# WHAT THIS IS FOR, AND WHY IT IS NOT selftest.sh. `preflight.sh` proves the
# wiring, `selftest.sh` and `plasmatest.sh` prove the decisions; none of them
# has ever hovered a button. A panel that does not reach the screen edge, a
# popup that opens across the panel, a tooltip that takes the click meant for
# its button, a toast that steals the keyboard: those are SCREEN defects, and
# the only thing that finds one is a screen with a hand on it.
#
# IT ASSERTS NOTHING. It produces a numbered contact sheet and a checklist to
# read it against — testing/usability.md, one heading per shot. A pass is a
# person looking; what is automated here is the driving, which is the part
# nobody does twice the same way.
#
# EVERY STEP IS THE POINTER OR A CHORD, never a command typed on a console,
# except the one notification, typed into the Konsole the run opened.
# ---------------------------------
set -eu

cd "$(dirname "$0")/.."

SIZE=${1:-1280x800}
OUT=${KDOS_USABILITY_OUT:-build/shots/usability}
ISO=build/iso-build/kdos.iso

[ -f "$ISO" ] || { echo "usability: no $ISO — run a full build once" >&2; exit 1; }
mkdir -p "$OUT"
rm -f "$OUT"/*.png "$OUT"/*.ppm

#
# WHERE THE PANEL IS, IN PIXELS, AT THIS SIZE.
#
# The KDOS desk (src/desktop/kdos-plasma-look/desks/org.kdos.desk.kdos) puts
# one opaque, non-floating panel on the bottom edge, 2 * ceil(gridUnit * 2.5 / 2)
# pixels tall: forty-odd at the default 10-point font and scale 1. Its centre
# line is aimed at as PANEL_Y, twenty pixels above the bottom edge, which stays
# inside the panel at any font size the desk is laid out with. Left to right it
# holds Kickoff (the mark and the word Start), the pager, the pinned apps, the
# meters, the tray, the clock and Show Desktop, one panel-height square, at the
# right edge; the clock is aimed at just left of that square.
#
# A run at another font size, scale or desk has a different panel, and the
# numbers come back out of a fresh 02-desktop shot. The tell is a --click on
# the panel that opens nothing.
#
W=${SIZE%x*}
H=${SIZE#*x}
PANEL_Y=$((H - 20))				# the panel's centre line
START_X=40					# on Kickoff's mark and label
CLOCK_X=$((W - 110))				# inside the clock
DESK_X=$((W / 2))				# bare desktop, above the panel
DESK_Y=$((H / 3))

echo "==> $SIZE: the panel at y=$PANEL_Y, Start at x=$START_X, the clock at x=$CLOCK_X"

exec docker run --rm --device /dev/kvm -v "$PWD:/kdos" -w /kdos \
	kdos-qemu-py:latest python3 testing/vnc-shot.py --size "$SIZE" \
	\
	--sleep 5 --shot "/kdos/$OUT/01-welcome.png" \
	--dismiss-picker --sleep 2 --shot "/kdos/$OUT/02-desktop.png" \
	\
	--mouse "$START_X,$PANEL_Y" --sleep 2 \
	--shot "/kdos/$OUT/03-start-hover.png" \
	--click "$START_X,$PANEL_Y" --sleep 3 \
	--shot "/kdos/$OUT/04-start-menu.png" \
	--mouse "200,$((H / 2))" --sleep 2 --shot "/kdos/$OUT/05-menu-hover.png" \
	--mouse "$((START_X + 200)),$PANEL_Y" --sleep 2 \
	--shot "/kdos/$OUT/06-menu-over-the-panel.png" \
	--keys esc --sleep 2 --shot "/kdos/$OUT/07-menu-closed.png" \
	\
	--mouse "$CLOCK_X,$PANEL_Y" --sleep 2 \
	--shot "/kdos/$OUT/08-clock-hover.png" \
	--click "$CLOCK_X,$PANEL_Y" --sleep 3 \
	--shot "/kdos/$OUT/09-calendar.png" \
	--keys esc --sleep 2 \
	\
	--chord meta+ret --sleep 4 --shot "/kdos/$OUT/10-terminal.png" \
	--mouse "$((W / 2)),$PANEL_Y" --sleep 2 \
	--shot "/kdos/$OUT/11-panel-with-a-window.png" \
	\
	--click "$DESK_X,$DESK_Y,3" --sleep 2 --shot "/kdos/$OUT/12-desktop-menu.png" \
	--keys esc --sleep 1 \
	--chord meta+space --sleep 3 --shot "/kdos/$OUT/13-search.png" \
	--keys esc --sleep 1 \
	\
	`# THE KONSOLE HAS TO BE GIVEN THE FOCUS BACK FIRST: the desktop menu` \
	`# and the search above took it, and a --type then goes nowhere.` \
	--click "$((W / 2)),$((H / 2))" --sleep 1 \
	--type 'kdos notify "Usability" "a toast takes no keyboard"' \
	--shot "/kdos/$OUT/14-toast.png" \
	--sleep 8 --shot "/kdos/$OUT/15-toast-gone.png"
