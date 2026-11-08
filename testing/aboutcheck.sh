#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   aboutcheck — every About-page row program prints two lines, fast
# ---------------------------------
#
#   testing/aboutcheck.sh                 on a booted KDOS, as any user
#   testing/aboutcheck.sh --root DIR      inside a built tree, as root (chroot)
#   testing/aboutcheck.sh --selftest      the proof each verdict can fail
#
# kinfocenter runs every program kcm-about-distrorc names in ExtraSoftwareData
# twice, synchronously on the GUI thread, once in the session's language and
# once under LANGUAGE=en_US:C, and calls qFatal on fewer than two lines of
# output — which takes System Settings down with the About page. So each
# program is held to:
#
#   it exists and is executable          a missing one prints nothing
#   exactly two non-empty lines          a label and a value
#   under 100 ms, every run              it runs on the GUI thread
#
# in each of the conditions it meets on a real machine: the session's
# environment, LANGUAGE=en_US:C, a single-root machine with no A/B boot state
# (/boot/efi/EFI/kdos/bootstate absent), and /var read-only. The last two need
# a private mount namespace, so they run as root (unshare -m) and are reported
# as skipped otherwise.
#
# Exit 0 every program holds, 1 one does not, 2 a usage error or no rc file.

set -u
cd "$(dirname "$0")/.."

LIMIT_MS=100
ROOT=""
RC=""
fail=0

usage() {
	echo "usage: testing/aboutcheck.sh [--root DIR] [--rc FILE] | --selftest" >&2
	exit 2
}

# Milliseconds from bash's own clock: date's %N is not in every date this tree
# ships, and one that prints it literally makes every run look slow.
now_ms() {
	_e=${EPOCHREALTIME//[.,]/}
	echo "${_e%???}"
}

# run_one PROGRAM CONDITION [ENV...]: one run, judged.
run_one() {
	_p=$1
	_c=$2
	shift 2
	_t0=$(now_ms)
	if [ -n "$ROOT" ]; then
		_out=$(env "$@" chroot "$ROOT" "$_p" 2>/dev/null)
	else
		_out=$(env "$@" "$_p" 2>/dev/null)
	fi
	_ms=$(($(now_ms) - _t0))
	_lines=$(printf '%s\n' "$_out" | grep -c .)
	_total=$(printf '%s\n' "$_out" | wc -l)
	if [ "$_lines" -ne 2 ] || [ "$_total" -ne 2 ]; then
		echo "  FAIL  $_p ($_c): $_lines non-empty of $_total line(s), not a label and a value"
		fail=1
	elif [ "$_ms" -ge "$LIMIT_MS" ]; then
		echo "  FAIL  $_p ($_c): ${_ms} ms, the limit is ${LIMIT_MS} ms"
		fail=1
	else
		printf '  ok    %-34s %-14s %3d ms  %s\n' "$_p" "($_c)" "$_ms" \
			"$(printf '%s' "$_out" | tr '\n' '|')"
	fi
}

# The conditions a mount namespace makes: no boot state, a read-only /var.
run_degraded() {
	_p=$1
	if [ "$(id -u)" -ne 0 ] || ! command -v unshare >/dev/null 2>&1; then
		echo "  skip  $_p (no bootstate, read-only /var): needs root and unshare"
		return
	fi
	_r=${ROOT:-/}
	for _c in nobootstate rovar; do
		_t0=$(now_ms)
		case $_c in
		nobootstate)
			_out=$(unshare -m sh -c '
				r=$1; p=$2
				[ -d "$r/boot/efi/EFI/kdos" ] &&
					mount -t tmpfs -o ro kdos-aboutcheck "$r/boot/efi/EFI/kdos"
				exec chroot "$r" "$p"' sh "$_r" "$_p" 2>/dev/null) ;;
		rovar)
			_out=$(unshare -m sh -c '
				r=$1; p=$2
				mount --bind "$r/var" "$r/var" &&
					mount -o remount,bind,ro "$r/var" &&
					exec chroot "$r" "$p"' sh "$_r" "$_p" 2>/dev/null) ;;
		esac
		_ms=$(($(now_ms) - _t0))
		_lines=$(printf '%s\n' "$_out" | grep -c .)
		_total=$(printf '%s\n' "$_out" | wc -l)
		if [ "$_lines" -ne 2 ] || [ "$_total" -ne 2 ]; then
			echo "  FAIL  $_p ($_c): $_lines non-empty of $_total line(s)"
			fail=1
		elif [ "$_ms" -ge "$LIMIT_MS" ]; then
			echo "  FAIL  $_p ($_c): ${_ms} ms, the limit is ${LIMIT_MS} ms"
			fail=1
		else
			printf '  ok    %-34s %-14s %3d ms  %s\n' "$_p" "($_c)" "$_ms" \
				"$(printf '%s' "$_out" | tr '\n' '|')"
		fi
	done
}

# The programs an rc file names, one a line.
programs() {
	sed -n 's/^[[:space:]]*ExtraSoftwareData[[:space:]]*=//p' "$1" | tail -1 |
		tr ';' '\n' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' | grep .
}

check_rc() {
	_rc=$1
	_n=0
	for _p in $(programs "$_rc"); do
		_n=$((_n + 1))
		_on=$_p
		[ -n "$ROOT" ] && _on=$ROOT$_p
		if [ ! -x "$_on" ]; then
			echo "  FAIL  $_p: not installed or not executable; kinfocenter would qFatal"
			fail=1
			continue
		fi
		run_one "$_p" session
		run_one "$_p" en_US:C LANGUAGE=en_US:C
		run_degraded "$_p"
	done
	[ "$_n" -gt 0 ] || echo "  note  $_rc names no ExtraSoftwareData program"
	echo "aboutcheck: $_n program(s) in $_rc"
}

selftest() {
	_d=$(mktemp -d)
	_good=$_d/good _one=$_d/one _slow=$_d/slow _gap=$_d/gap
	printf '#!/bin/sh\necho Accent\necho Bone\n' > "$_good"
	printf '#!/bin/sh\necho Accent\n' > "$_one"
	printf '#!/bin/sh\nsleep 0.3\necho Accent\necho Bone\n' > "$_slow"
	printf '#!/bin/sh\necho Accent\necho\necho Bone\n' > "$_gap"
	chmod +x "$_good" "$_one" "$_slow" "$_gap"
	_ok=1
	for _case in "good:0" "one:1" "slow:1" "gap:1" "missing:1"; do
		_name=${_case%%:*}
		_want=${_case#*:}
		printf '[General]\nExtraSoftwareData=%s\n' "$_d/$_name" > "$_d/rc"
		fail=0
		check_rc "$_d/rc" >/dev/null
		if [ "$fail" = "$_want" ]; then
			echo "  ok    $_name: $([ "$_want" = 1 ] && echo fails || echo passes)"
		else
			echo "  FAIL  $_name: fail=$fail, wanted $_want"
			_ok=0
		fi
	done
	rm -rf "$_d"
	[ "$_ok" = 1 ]
}

while [ $# -gt 0 ]; do
	case $1 in
	--root) ROOT=${2:-}; [ -n "$ROOT" ] || usage; shift 2 ;;
	--rc) RC=${2:-}; [ -n "$RC" ] || usage; shift 2 ;;
	--selftest) selftest; exit $? ;;
	*) usage ;;
	esac
done

if [ -n "$ROOT" ] && [ "$(id -u)" -ne 0 ]; then
	echo "aboutcheck: --root runs the programs in a chroot, which needs root" >&2
	exit 2
fi
if [ -z "$RC" ]; then
	for _c in "$ROOT/etc/xdg/kcm-about-distrorc" "$ROOT/usr/share/kdos/xdg/kcm-about-distrorc"; do
		[ -f "$_c" ] && { RC=$_c; break; }
	done
fi
[ -n "$RC" ] && [ -f "$RC" ] || { echo "aboutcheck: no kcm-about-distrorc found" >&2; exit 2; }

check_rc "$RC"
exit $fail
