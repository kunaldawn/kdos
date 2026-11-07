#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/atspi/golden.sh — the AT-SPI gate on the shipped image
#
#   testing/atspi/golden.sh [--update] [--webdriver] [SURFACE...]
#
# Boots build/iso-build/kdos.iso on the rig, dumps every SURFACE (all of
# testing/atspi/surfaces when none is named) inside the Plasma session with
# testing/atspi/guest.sh, brings the dumps back to build/atspi/dumps, and runs
# `atspi.py check` on them: each must match its golden in testing/atspi/goldens
# and hold no visible control without an accessible name and no host error
# page in place of the surface. --update writes the dumps as the goldens
# instead, refusing any that fails those checks; this is
# the only thing that writes a golden, because the goldens describe the
# shipped image. --webdriver dumps through selenium-webdriver-at-spi in the
# guest instead of reading the bus directly.
#
# The harness travels in as a tar of testing/atspi on a data disk, and the
# dumps come out as a tar written over a scratch disk this marks with
# KDOS-RIG-SCRATCH, which is how the guest tells the two apart.
# ---------------------------------
set -eu

cd "$(dirname "$0")/../.."
update=""
guestargs=""
while [ $# -gt 0 ]; do
	case $1 in
	--update) update=--update ;;
	--webdriver) guestargs="$guestargs --webdriver" ;;
	-*) echo "golden: unknown option $1" >&2; exit 64 ;;
	*) break ;;
	esac
	shift
done
for s in "$@"; do
	[ -f "testing/atspi/surfaces/$s.json" ] ||
		{ echo "golden: no surface $s (python3 testing/atspi/atspi.py list)" >&2; exit 64; }
done
[ -f build/iso-build/kdos.iso ] ||
	{ echo "golden: no build/iso-build/kdos.iso — run a full build once" >&2; exit 1; }

dir=build/atspi
mkdir -p "$dir" build/shots
rm -rf "$dir/dumps"
mkdir -p "$dir/dumps"
tar cf "$dir/in.tar" testing/atspi
rm -f "$dir/out.img"
truncate -s 64M "$dir/out.img"
printf KDOS-RIG-SCRATCH | dd of="$dir/out.img" conv=notrunc status=none

rc=0
# shellcheck disable=SC2086
docker run --rm --device /dev/kvm -v "$PWD:/kdos" -w /kdos kdos-qemu-py:latest \
	python3 testing/vnc-shot.py --size 1920x1080 \
	--data-disk "/kdos/$dir/in.tar" --scratch "/kdos/$dir/out.img" \
	--dismiss-picker \
	--root-script "testing/atspi/guest.sh$guestargs $*" \
	--shot /kdos/build/shots/atspi.ppm || rc=$?

if ! tar xf "$dir/out.img" -C "$dir/dumps" 2>/dev/null; then
	echo "golden: the guest wrote no dumps (vnc-shot exited $rc)" >&2
	exit 1
fi
for e in "$dir"/dumps/*.err; do
	[ -s "$e" ] || continue
	echo "--- ${e##*/}"
	tail -n 5 "$e"
done
python3 -I testing/atspi/atspi.py check "$dir/dumps" $update "$@" || rc=1
exit "$rc"
