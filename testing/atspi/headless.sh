#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/atspi/headless.sh — AT-SPI dumps in the development image, with
#   no VM
#
#   testing/atspi/headless.sh [--image IMG] [--prefix DIR] [--runs N]
#                             [--out DIR] SURFACE...
#
# Starts a session bus, the AT-SPI bus and a headless Wayland compositor in
# the development image, dumps each SURFACE with testing/atspi/atspi.py the way
# the rig does, and checks the dumps against testing/atspi/goldens — a minute's
# loop for a KCM's tree instead of a boot.
#
# THE COMPOSITOR IS WESTON'S HEADLESS BACKEND, NOT KWIN. KWin's virtual
# backend wants a render device a container has not got, and quits — taking
# every client with it — the first time it has to draw its own QML. weston
# renders with pixman and needs nothing. A tree is the client's, so the
# compositor does not change it; what it does change is anything the surface
# asks KWin itself over D-Bus, which a KWin KCM then reports as an error. --runs N dumps everything N times
# and fails unless every run printed the same bytes, which is how a surface
# is shown to be deterministic before its golden is trusted. --prefix DIR is
# an install root (a port's $PKG, plasmatest's prefix) whose plugins, QML
# modules and programs are put first, so a KDOS KCM built there can be dumped.
#
# THE GOLDENS ARE THE RIG'S. They are written only by testing/atspi/golden.sh,
# from the shipped image; this never writes one. The development image
# carries Alpine's KDE build, a point release away from KDOS's at times, so a
# difference here against a golden is either the change under test or the
# two builds disagreeing, and a golden.sh run says which.
#
# IMG defaults to $KDOS_ATSPI_IMAGE, then kdos-devdeps:latest. It needs
# weston with its headless backend and desktop shell, dbus-run-session,
# at-spi2-core and python3's gi with Atspi; the dumps are written under --out
# (default build/atspi/headless).
# ---------------------------------
set -eu

if [ "${1:-}" = --inner ]; then
	shift
	# ── inside the container ─────────────────────────────────────────────
	# Started as root, it makes an account with the caller's uid and runs
	# itself again as that account: dbus-daemon refuses a uid with no passwd
	# entry, KWin runs unprivileged as it does in a session, and the dumps on
	# the bind mount belong to the caller.
	if [ "$(id -u)" = 0 ]; then
		u=${RIG_UID:-4242}
		getent passwd "$u" >/dev/null 2>&1 ||
			adduser -D -u "$u" rig >/dev/null 2>&1 ||
			useradd -m -u "$u" rig
		name=$(getent passwd "$u" | cut -d: -f1)
		RIG_ARGS="$*"
		export RIG_ARGS
		# shellcheck disable=SC2016 # expanded by the account's shell
		exec su -s /bin/sh "$name" -c 'exec sh /kdos/testing/atspi/headless.sh --inner $RIG_ARGS'
	fi
	export HOME=/tmp/rig-home
	mkdir -p "$HOME"
	runs=$1 prefix=$2
	shift 2
	[ "$prefix" = - ] && prefix=""
	export XDG_RUNTIME_DIR=/tmp/rig-run
	mkdir -p "$XDG_RUNTIME_DIR"
	chmod 700 "$XDG_RUNTIME_DIR"
	if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
		exec dbus-run-session -- sh "$0" --inner "$runs" "$prefix" "$@"
	fi
	if [ -n "$prefix" ] && [ -d "$prefix" ]; then
		PATH=$prefix/usr/bin:$PATH
		export QT_PLUGIN_PATH="$prefix/usr/lib/qt6/plugins"
		export QML_IMPORT_PATH="$prefix/usr/lib/qt6/qml"
		export XDG_DATA_DIRS="$prefix/usr/share:/usr/local/share:/usr/share"
		# Only a prefix that installs libraries of its own gets a library
		# path: kcmshell6 run with one naming a directory of plugins alone
		# exits without opening its KCM.
		if ls "$prefix"/usr/lib/lib*.so* >/dev/null 2>&1; then
			export LD_LIBRARY_PATH="$prefix/usr/lib"
		fi
	fi
	# The a11y bus is started by D-Bus activation; IsEnabled is what tells
	# every toolkit to publish its tree on it.
	dbus-send --session --print-reply --dest=org.a11y.Bus /org/a11y/bus \
		org.freedesktop.DBus.Properties.Set string:org.a11y.Status \
		string:IsEnabled variant:boolean:true >/dev/null
	# Qt Quick draws in software: a container has no GPU, and a tree does
	# not depend on the renderer.
	export QT_QUICK_BACKEND=software
	weston --backend=headless --renderer=pixman --socket=wayland-rig \
		--width=1280 --height=800 > /out/compositor.log 2>&1 &
	comp=$!
	i=0
	while [ ! -S "$XDG_RUNTIME_DIR/wayland-rig" ] && [ $i -lt 60 ]; do
		sleep 0.5
		i=$((i + 1))
	done
	export WAYLAND_DISPLAY=wayland-rig QT_QPA_PLATFORM=wayland
	export XDG_CURRENT_DESKTOP=KDE
	rc=0
	r=1
	while [ "$r" -le "$runs" ]; do
		mkdir -p "/out/run$r"
		for s in "$@"; do
			if python3 -I /kdos/testing/atspi/atspi.py dump "$s" \
				--out "/out/run$r/$s.txt" 2> "/out/run$r/$s.err"; then
				echo "dumped $s (run $r)"
			else
				echo "FAIL  $s (run $r): $(tail -n 1 "/out/run$r/$s.err")"
				rc=1
			fi
		done
		r=$((r + 1))
	done
	kill "$comp" 2>/dev/null
	exit "$rc"
fi

# ── on the host ─────────────────────────────────────────────────────────
image=${KDOS_ATSPI_IMAGE:-kdos-devdeps:latest}
prefix=""
runs=1
out=""
while [ $# -gt 0 ]; do
	case $1 in
	--image) image=$2; shift ;;
	--prefix) prefix=$(cd "$2" && pwd); shift ;;
	--runs) runs=$2; shift ;;
	--out) out=$2; shift ;;
	-*) echo "headless: unknown option $1" >&2; exit 64 ;;
	*) break ;;
	esac
	shift
done
[ $# -gt 0 ] || { echo "usage: testing/atspi/headless.sh [--runs N] SURFACE... (atspi.py list)" >&2; exit 64; }
case $runs in '' | *[!0-9]* | 0) echo "headless: --runs takes a count" >&2; exit 64 ;; esac
cd "$(dirname "$0")/../.."
out=${out:-build/atspi/headless}
rm -rf "$out"
mkdir -p "$out"
out=$(cd "$out" && pwd)
pmount=""
[ -n "$prefix" ] && pmount="-v $prefix:/prefix:ro"
# shellcheck disable=SC2086
docker run --rm --network none -e RIG_UID="$(id -u)" \
	-v "$PWD:/kdos:ro" -v "$out:/out" $pmount \
	"$image" sh /kdos/testing/atspi/headless.sh --inner "$runs" \
	"${prefix:+/prefix}${prefix:--}" "$@" || true

rc=0
r=2
while [ "$r" -le "$runs" ]; do
	if diff -r "$out/run1" "$out/run$r" -x '*.err' > "$out/run$r.diff"; then
		echo "ok    run $r printed the same trees as run 1"
	else
		echo "FAIL  run $r differs from run 1 ($out/run$r.diff)"
		cat "$out/run$r.diff"
		rc=1
	fi
	r=$((r + 1))
done
python3 -I testing/atspi/atspi.py check "$out/run1" "$@" || rc=1
exit "$rc"
