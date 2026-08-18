#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   Does every alien app actually start?
#
#   testing/appbox-smoke.sh              every app in the table
#   testing/appbox-smoke.sh freecad gimp just these
#   testing/appbox-smoke.sh --probe      the cheap pass only
#
# WHY. There are ~92 launchers on the Start menu and nothing has ever checked
# that any of them opens anything. They are generated from the image's own
# desktop entries, so a defect in the generator, a package that did not make it
# into the bake, or an Exec line the launcher mis-parses all look identical from
# the desktop: you click the icon and nothing happens, silently, forever.
#
# TWO PASSES, AND THE CHEAP ONE FIRST. Asking `command -v` for every entry is
# one container enter for the whole table and it answers the largest single
# failure class — the launcher exists and the program behind it does not —
# in seconds rather than in half an hour. Only what survives that is worth
# actually launching.
#
# WHAT COUNTS AS WORKING. The app is started with a timeout and no file
# argument; if it is STILL ALIVE when the timeout fires (rc 124) it drew a
# window and stayed up, which is the thing being tested. An app that exits by
# itself before then did not: a missing library, a bad argv, a display it could
# not reach. rc 0 is reported separately and is NOT a pass — a program that
# printed its version and exited did not open.
#
# Run it INSIDE the session, as the desktop user:
#   su - kdos -c 'WAYLAND_DISPLAY=wayland-0 testing/appbox-smoke.sh'
# ---------------------------------

set -u

TABLE=${KDOS_ALIEN_APPS:-/usr/share/kdos/alien-apps}
BOX=${KDOS_APPBOX:-kdos-apps}
LAUNCH_TIMEOUT=${LAUNCH_TIMEOUT:-20}
OUT=${OUT:-/tmp/appbox-smoke}
probe_only=0

want=()
for a in "$@"; do
	case "$a" in
	--probe) probe_only=1 ;;
	-*) echo "usage: appbox-smoke.sh [--probe] [app ...]" >&2; exit 2 ;;
	*) want+=("$a") ;;
	esac
done

[ -r "$TABLE" ] || { echo "no alien-app table at $TABLE" >&2; exit 2; }
mkdir -p "$OUT"

# name<TAB>command, comments dropped. The command keeps its quoting: it is
# read back by kdos-appbox, which splits it the way a .desktop Exec is split.
mapfile -t rows < <(grep -v '^#' "$TABLE" | grep -v '^$')

selected() {
	[ ${#want[@]} -eq 0 ] && return 0
	local n=$1 w
	for w in "${want[@]}"; do [ "$w" = "$n" ] && return 0; done
	return 1
}

# ── pass 1: does the binary exist in the box at all ──────────────────────
#
# One enter for the whole table. The first word of the command is the program;
# `sh -c "..."` is asked about `sh`, which always exists, so those fall through
# to the launch pass where they belong.
echo "==> probing $BOX for the programs behind ${#rows[@]} launchers"
: > "$OUT/probe.txt"
{
	for row in "${rows[@]}"; do
		name=${row%%$'\t'*}
		cmd=${row#*$'\t'}
		selected "$name" || continue
		# The first word, with any surrounding quotes stripped — the
		# table quotes an argument containing a space, and `command -v`
		# would look for a file whose name begins with one.
		bin=${cmd%% *}
		bin=${bin#\"}; bin=${bin%\"}
		printf 'if command -v %q >/dev/null 2>&1; then echo "OK %s"; else echo "NOBIN %s"; fi\n' \
			"$bin" "$name" "$name"
	done
} > "$OUT/probe.sh"

if [ ! -s "$OUT/probe.sh" ]; then
	echo "nothing selected" >&2
	exit 2
fi
distrobox enter "$BOX" -- bash -s < "$OUT/probe.sh" > "$OUT/probe.txt" 2>"$OUT/probe.err"

nobin=$(grep -c '^NOBIN ' "$OUT/probe.txt" 2>/dev/null || echo 0)
okbin=$(grep -c '^OK ' "$OUT/probe.txt" 2>/dev/null || echo 0)
echo "    $okbin present, $nobin missing from the image"
grep '^NOBIN ' "$OUT/probe.txt" | sed 's/^NOBIN /    missing: /'

if [ "$probe_only" = 1 ]; then
	exit 0
fi

# ── pass 2: does it stay up ──────────────────────────────────────────────
#
# Through `kdos-appbox run`, not through distrobox directly: the environment
# the launchers get (WAYLAND_DISPLAY, the portal variables, the Qt platform
# theme, DISPLAY for the X11-only ones) is what kdos-appbox exports, and
# testing anything else would be testing a path no launcher takes.
: > "$OUT/result.txt"
for row in "${rows[@]}"; do
	name=${row%%$'\t'*}
	selected "$name" || continue
	if grep -qx "NOBIN $name" "$OUT/probe.txt"; then
		echo "MISSING $name -" >> "$OUT/result.txt"
		printf '  %-22s MISSING (no such program in the image)\n' "$name"
		continue
	fi

	# The shim: `kdos-appbox` invoked under the app's own name, which is
	# exactly what a .desktop launcher and a terminal both reach.
	timeout -k 3 "$LAUNCH_TIMEOUT" kdos-appbox run "$name" \
		> "$OUT/$name.log" 2>&1 &
	pid=$!
	wait $pid
	rc=$?
	case $rc in
	124|137)
		echo "UP $name $rc" >> "$OUT/result.txt"
		printf '  %-22s up\n' "$name"
		;;
	0)
		echo "EXIT0 $name 0" >> "$OUT/result.txt"
		printf '  %-22s exited 0 — did it open?\n' "$name"
		;;
	*)
		echo "FAIL $name $rc" >> "$OUT/result.txt"
		printf '  %-22s FAILED rc=%s: %s\n' "$name" "$rc" \
			"$(tail -1 "$OUT/$name.log" 2>/dev/null | cut -c1-70)"
		;;
	esac
	# Anything it left behind. A launcher that opened a window and was
	# killed can leave the app running, and ninety of those is a machine
	# with no memory left by the end of the run.
	pkill -u "$(id -u)" -f "$name" >/dev/null 2>&1 || true
done

echo
echo "==> summary"
printf '    %-10s %s\n' up      "$(grep -c '^UP '      "$OUT/result.txt" || true)"
printf '    %-10s %s\n' exit0   "$(grep -c '^EXIT0 '   "$OUT/result.txt" || true)"
printf '    %-10s %s\n' failed  "$(grep -c '^FAIL '    "$OUT/result.txt" || true)"
printf '    %-10s %s\n' missing "$(grep -c '^MISSING ' "$OUT/result.txt" || true)"
echo "    per-app output in $OUT"
grep -E '^(FAIL|MISSING) ' "$OUT/result.txt" | sed 's/^/    /'
