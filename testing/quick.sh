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
#   testing/quick.sh plasmoid-kdos-privacy -- --sleep 3 \
#                                             --shot /kdos/build/shots/x.png
#
# WHAT IT SAVES, AND WHY THAT IS THE WHOLE POINT. The ordinary loop is
# `make build` with 70_image, which repacks a 32 GB ISO — most of the build's
# time — to carry a 200 KB plugin. This builds the named ports into `build/fs`
# with NO 70_image, tars exactly the files those ports own, and hands them to a
# booted ISO on a raw disk — where `quickpatch.sh` untars them over the live
# medium's RAM overlay and restarts what reads them: KWin for KWin and its
# plugins, plasmashell for a plasmoid or a Plasma package, an effect, a KWin
# script or a kded module in place. quickpatch.sh holds the whole table.
#
# THE FILE LIST IS THE PACKAGE DATABASE'S, not a guess: `build/fs/var/lib/kpkg/
# db/<port>` is what that port installed, so a program that grew a new name or
# a new data file is carried without anyone remembering to add it here.
#
# WHAT IT IS NOT FOR. Anything under `fs/`, a new port, a kernel change or an
# initramfs change is not in a package's file list and does not reach the guest
# this way — those need the real build. Nor does an SDDM theme: the live
# session is already logged in, and no greeter runs to read it. Nor anything
# that owns a D-Bus NAME on the session bus: a restarted owner races the old
# one for the name and clients keep talking to the old one, so a daemon is
# checked on a booted ISO. And nothing this produces is evidence about the ISO:
# the shipped image is what `make build` writes, and a wave's verification
# photograph is taken from that. This is the loop you iterate in before you
# take that photograph.
#
# The run waits for vnc-shot.py's Plasma gate, closes the first-run welcome
# window, patches, and then runs the steps after `--`.
#
# KDOS_QUICK_KEEP=1 leaves the session alone. Right whenever the program under
# test is SPAWNED — a KCM, Discover, the installer, anything a step launches
# fresh — so the new code runs without a restart.
# KDOS_QUICK_RESTART=kwin,plasmashell,kded,krunner restarts exactly those,
# whatever the tar holds; the default, auto, follows the tar.
# ---------------------------------
set -eu

cd "$(dirname "$0")/.."

PORTS=${1:-}
if [ -z "$PORTS" ]; then
	echo "usage: testing/quick.sh PORT[,PORT...] [-- vnc-shot args...]" >&2
	echo "       KDOS_QUICK_PHASES=43_toolkits,50_desktop to widen the build" >&2
	echo "       KDOS_QUICK_NOBUILD=1 to reuse what is already in build/fs" >&2
	echo "       KDOS_QUICK_KEEP=1 to patch without restarting anything" >&2
	echo "       KDOS_QUICK_RESTART=kwin,plasmashell to choose the restarts" >&2
	exit 2
fi
shift
[ "${1:-}" = "--" ] && shift

PHASES=${KDOS_QUICK_PHASES:-50_desktop}
# Extra paths under build/fs to carry — a config file, a default under
# /usr/share/kdos/xdg. The fs step must have run for them to be there: they
# come from the tree's `fs/`, not from a package.
FILES=${KDOS_QUICK_FILES:-}
RESTART=${KDOS_QUICK_RESTART:-auto}
case $RESTART in
*[!a-z,]*) echo "quick: KDOS_QUICK_RESTART is auto, none or a list of kwin,plasmashell,kded,krunner" >&2; exit 2 ;;
esac
ISO=build/iso-build/kdos.iso
TAR=build/fix.tar

[ -f "$ISO" ] || { echo "quick: no $ISO — run a full build once" >&2; exit 1; }

# ── 1. build, without 70_image ──────────────────────────────────────────
if [ "${KDOS_QUICK_NOBUILD:-0}" != 1 ]; then
	echo "==> building $PORTS ($PHASES, no image)"
	make build BUILD_ARGS="--phases $PHASES --rebuild $PORTS"
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
	# fs step installs there too, so a config file can ride along with
	# the binaries that read it.
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
# inherits nothing from this shell, so the choices have to be written into
# the copy it receives.
sed -e "s/^KEEP=0$/KEEP=${KDOS_QUICK_KEEP:-0}/" \
    -e "s/^SKEL=0$/SKEL=${KDOS_QUICK_SKEL:-0}/" \
    -e "s/^RESTART=auto$/RESTART=$RESTART/" testing/quickpatch.sh \
	> build/quickpatch.gen.sh
chmod +x build/quickpatch.gen.sh

echo "==> booting"
exec docker run --rm --device /dev/kvm -v "$PWD:/kdos" -w /kdos \
	kdos-qemu-py:latest python3 testing/vnc-shot.py --size 1280x800 \
	--data-disk /kdos/build/fix.tar --dismiss-picker \
	--root-script /kdos/build/quickpatch.gen.sh "$@"
