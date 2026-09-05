#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   The fast loop: change a program, see it on a booted desktop.
#
#   testing/quick.sh kdos-con,kdos-shell -- --keys meta_l-ret --sleep 3 \
#                                            --shot /kdos/build/shots/x.png
#
# WHAT IT SAVES, AND WHY THAT IS THE WHOLE POINT. The ordinary loop is
# `make build` with 06_packaging, which repacks a 32 GB ISO: measured 5m30s of
# a 7m30s build, to carry a 200 KB binary. This builds the named ports into
# `build/fs` with NO packaging (measured 1m09s), tars exactly the files those
# ports own, and hands them to a booted ISO on a raw disk — where
# `quickpatch.sh` untars them over the live medium's RAM overlay and restarts
# the session. Twelve minutes becomes three.
#
# THE FILE LIST IS THE PACKAGE DATABASE'S, not a guess: `build/fs/var/lib/kpkg/
# db/<port>` is what that port installed, so a program that grew a new name or
# a new data file is carried without anyone remembering to add it here.
#
# WHAT IT IS NOT FOR. Anything under `fs/`, a new port, a kernel change or an
# initramfs change is not in a package's file list and does not reach the guest
# this way — those need the real build. Neither does anything that owns a D-Bus
# NAME: `quickpatch.sh` restarts the session, and a notification raised in a
# restarted session has never been seen to draw while the same call on a booted
# ISO does. And nothing this produces is evidence about the ISO: the shipped
# image is what `make build` writes, and a wave's verification photograph is
# taken from that. This is the loop you iterate in before you take that
# photograph.
#
# KDOS_QUICK_KEEP=1 leaves the session alone. Right whenever the program under
# test is SPAWNED — every `kdos-shell` surface is started fresh by the chord
# that opens it, so the new binary runs without a restart and the run is a
# minute shorter.
# ---------------------------------
set -eu

cd "$(dirname "$0")/.."

PORTS=${1:-}
if [ -z "$PORTS" ]; then
	echo "usage: testing/quick.sh PORT[,PORT...] [-- vnc-shot args...]" >&2
	echo "       KDOS_QUICK_PHASES=04_phase4,05_desktop to widen the build" >&2
	echo "       KDOS_QUICK_NOBUILD=1 to reuse what is already in build/fs" >&2
	exit 2
fi
shift
[ "${1:-}" = "--" ] && shift

PHASES=${KDOS_QUICK_PHASES:-05_desktop}
# Extra paths under build/fs to carry — a config file, a chord table.
# The fs step must have run for them to be there: they come from the
# tree's `fs/`, not from a package.
FILES=${KDOS_QUICK_FILES:-}
ISO=build/iso-build/kdos.iso
TAR=build/fix.tar

[ -f "$ISO" ] || { echo "quick: no $ISO — run a full build once" >&2; exit 1; }

# ── 1. build, without packaging ─────────────────────────────────────────
if [ "${KDOS_QUICK_NOBUILD:-0}" != 1 ]; then
	echo "==> building $PORTS ($PHASES, no packaging)"
	make build BUILD_ARGS="--phases $PHASES --rebuild $PORTS --no-snapshot"
fi

# ── 2. tar exactly what those ports own ─────────────────────────────────
#
# In a container because `build/fs` is root-owned: a build runs as root inside
# one, and reaching for sudo on the host to read it back is the thing CLAUDE.md
# tells you not to do.
echo "==> packing $TAR"
rm -f "$TAR"
docker run --rm -v "$PWD/build:/build" alpine sh -c '
	set -eu
	list=/tmp/files
	: > "$list"
	for p in $(echo "'"$PORTS"'" | tr "," " "); do
		db=/build/fs/var/lib/kpkg/db/$p
		[ -f "$db" ] || { echo "quick: no database entry for $p" >&2; exit 1; }
		# Directories and the bare "./" line are dropped: a tar of
		# directories would rewrite their modes on the guest, and an
		# empty path makes tar fail on the whole archive.
		tail -n +2 "$db" | sed "s|^\./||" | grep -v "/$" | grep . >> "$list"
	done
	# And whatever KDOS_QUICK_FILES named, relative to build/fs. The
	# fs step installs there too, so a config file or a chord table can
	# ride along with the binaries that read it.
	for f in '"$FILES"'; do
		[ -n "$f" ] || continue
		[ -e "/build/fs/$f" ] || { echo "quick: no /build/fs/$f" >&2; exit 1; }
		echo "$f" >> "$list"
	done
	sort -u "$list" > "$list.u"
	echo "quick: $(wc -l < "$list.u") files"
	tar cf /build/fix.tar -C /build/fs -T "$list.u"
'
ls -la "$TAR"

# ── 3. boot the ISO, patch it, run the caller's steps ───────────────────
#
# The patch script is generated rather than sent as it stands: the guest
# inherits nothing from this shell, so the keep-the-session choice has to be
# written into the copy it receives.
sed -e "s/^KEEP=0$/KEEP=${KDOS_QUICK_KEEP:-0}/" \
    -e "s/^SKEL=0$/SKEL=${KDOS_QUICK_SKEL:-0}/" testing/quickpatch.sh \
	> build/quickpatch.gen.sh
chmod +x build/quickpatch.gen.sh

echo "==> booting"
exec docker run --rm --device /dev/kvm -v "$PWD:/kdos" -w /kdos \
	kdos-qemu-py:latest python3 testing/vnc-shot.py --size 1280x800 \
	--no-session --sleep 40 --data-disk /kdos/build/fix.tar \
	--root-script /kdos/build/quickpatch.gen.sh \
	--keys esc --sleep 1 "$@"
