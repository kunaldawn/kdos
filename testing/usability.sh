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
#   testing/usability.sh 1920x1080    # a size whose cell count is not the same
#
# WHAT THIS IS FOR, AND WHY IT IS NOT selftest.sh. `preflight.sh` proves the
# wiring and `selftest.sh` proves the decisions; neither has ever hovered a
# button. Every defect this file exists to catch was invisible to both — a
# taskbar two rows above the bottom of the screen, a tooltip that swallowed the
# click on the button it was describing, a Start button whose label vanished
# when the pointer touched it, an icon layer eighty columns wide on a
# hundred-and-sixty column screen. They are all SCREEN bugs, and the only thing
# that finds a screen bug is a screen with a hand on it.
#
# IT ASSERTS NOTHING. It produces a numbered contact sheet and a checklist to
# read it against — testing/usability.md, one heading per shot. A pass is a
# person looking; what is automated here is the driving, which is the part
# nobody does twice the same way.
#
# EVERY STEP IS THE POINTER OR A CHORD, never a command typed on a console:
# the bugs live between the hit map and the draw, and a command reaches neither.
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
# WHERE THE BAR IS, IN PIXELS, AT THIS SIZE.
#
# The cell is 8x17 with the shipped face, the bar is its bottom three rows on
# this display (two of content and one of edge — see kdos-shell.md), and the
# row worth aiming at is the one the Start button's word is on. Derived rather
# than written down: a run at another size must aim at the same button.
#
# MEASURED FROM A SHOT AND NOT READ OUT OF THE FONT. The session picks its face
# through fcft at run time and prints no grid, so the only place this number
# exists is the picture: the pitch between two glyph rows in 01-welcome. A face
# change moves it, and the tell is a --click on the bar that opens nothing.
#
CELL_H=17
H=${SIZE#*x}
ROWS=$((H / CELL_H))
START_Y=$(((ROWS - 2) * CELL_H + CELL_H / 2))	# the bar's first content row
BELOW_Y=$(((ROWS - 1) * CELL_H + CELL_H / 2))	# its second
START_X=45					# inside the word `Start`
CLOCK_X=$(( ${SIZE%x*} - 60 ))			# inside the clock

echo "==> $SIZE: $ROWS rows, the bar's content at y=$START_Y and y=$BELOW_Y"

exec docker run --rm --device /dev/kvm -v "$PWD:/kdos" -w /kdos \
	kdos-qemu-py:latest python3 testing/vnc-shot.py --size "$SIZE" \
	--no-session --sleep 45 \
	\
	--shot "/kdos/$OUT/01-welcome.png" \
	--keys esc --sleep 2 --shot "/kdos/$OUT/02-desktop.png" \
	\
	--mouse "$START_X,$START_Y" --sleep 2 \
	--shot "/kdos/$OUT/03-start-hover.png" \
	--click "$START_X,$START_Y" --sleep 3 \
	--shot "/kdos/$OUT/04-start-menu.png" \
	--mouse 300,600 --sleep 2 --shot "/kdos/$OUT/05-menu-hover.png" \
	--mouse "$START_X,$BELOW_Y" --sleep 2 \
	--shot "/kdos/$OUT/06-menu-survives-the-bar.png" \
	--keys esc --sleep 2 --shot "/kdos/$OUT/07-menu-closed.png" \
	\
	--mouse "$CLOCK_X,$START_Y" --sleep 2 \
	--shot "/kdos/$OUT/08-clock-hover.png" \
	--click "$CLOCK_X,$START_Y" --sleep 3 \
	--shot "/kdos/$OUT/09-calendar.png" \
	--keys esc --sleep 2 \
	\
	--keys meta_l-ret --sleep 4 --shot "/kdos/$OUT/10-terminal.png" \
	--mouse "$START_X,$BELOW_Y" --sleep 1 \
	--shot "/kdos/$OUT/11-bar-with-a-window.png" \
	\
	--keys meta_l-f10 --sleep 3 --shot "/kdos/$OUT/12-root-menu.png" \
	--keys esc --sleep 1 \
	--chord super+space --sleep 3 --shot "/kdos/$OUT/13-search.png" \
	--keys esc --sleep 1 \
	\
	--click 900,300 --sleep 1 --shot "/kdos/$OUT/14-desktop-click.png" \
	--mouse 900,700 --sleep 1 --shot "/kdos/$OUT/15-desktop-lower.png" \
	\
	`# THE TERMINAL HAS TO BE GIVEN THE FOCUS BACK FIRST. The two steps` \
	`# above click bare desktop, which focuses the ICON LAYER — and a` \
	`# --type then goes to the icon layer's own type-ahead, so the` \
	`# notification is never raised and the shot below is of a desktop` \
	`# with nothing on it, every run.` \
	--click 400,300 --sleep 1 \
	--type 'kdos notify "Usability" "a toast takes no keyboard"' \
	--shot "/kdos/$OUT/16-toast.png" \
	--sleep 8 --shot "/kdos/$OUT/17-toast-gone.png"
