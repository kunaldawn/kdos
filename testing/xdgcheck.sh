#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   xdgcheck — KDOS's defaults stay out of the administrator's /etc/xdg
# ---------------------------------
#
#   testing/xdgcheck.sh [--root build/fs]
#
# Run from the repository root. Every KDOS default for a KDE program lives in
# /usr/share/kdos/xdg, the LAST directory of XDG_CONFIG_DIRS, so /etc/xdg
# stays the machine owner's: a key set there outranks the KDOS one, and an
# image update never collides with an edited /etc file. That holds only while
# nothing else ships a default into /etc/xdg, so this fails on:
#
#   a file under fs/etc/xdg the rulings do not put there. Allowed: kdeglobals
#     (the global theme alone, for a process that reads /etc/xdg alone),
#     konsolerc, fcitx5/config (fcitx5 reads one file, not a cascade),
#     mimeapps.list (the handlers that are not a desktop application's), and
#     the two directories the XDG specifications fix there, autostart/ and
#     menus/;
#   /etc/xdg/kdeglobals holding any key but [KDE] LookAndFeelPackage;
#   a file shipped under both fs/etc/xdg and fs/usr/share/kdos/xdg other than
#     kdeglobals: two copies of one default, the /etc one shadowing every key
#     it repeats;
#   a login shell whose XDG_CONFIG_DIRS is not /etc/xdg:/usr/share/kdos/xdg
#     (fs/etc/profile.d/10-wayland.sh, from an empty environment and from one
#     that already holds /etc/xdg);
#   with --root, a file the installer writes per machine (kxkbrc, kdosrc,
#     kwinrc) present in the image's etc/xdg: the image would carry one
#     machine's answer for every machine.
#
# Exit 0 clean, 1 a check fails, 2 a usage error.

set -u
cd "$(dirname "$0")/.."

ROOT=""
case ${1:-} in
--root) ROOT=${2:-}; [ -d "$ROOT/etc" ] || { echo "xdgcheck: $ROOT is not a built tree" >&2; exit 2; } ;;
"") ;;
*) echo "usage: testing/xdgcheck.sh [--root DIR]" >&2; exit 2 ;;
esac

fail=0
bad() { echo "  FAIL  $*"; fail=1; }

ALLOWED="kdeglobals konsolerc fcitx5/config mimeapps.list"
n=0
while IFS= read -r f; do
	rel=${f#fs/etc/xdg/}
	n=$((n + 1))
	case $rel in
	autostart/*.desktop | menus/*.menu | menus/applications-merged/*.menu) continue ;;
	esac
	case " $ALLOWED " in
	*" $rel "*) ;;
	*) bad "fs/etc/xdg/$rel: a KDOS default belongs in fs/usr/share/kdos/xdg/$rel" ;;
	esac
	[ -e "fs/usr/share/kdos/xdg/$rel" ] && [ "$rel" != kdeglobals ] &&
		bad "fs/etc/xdg/$rel and fs/usr/share/kdos/xdg/$rel: one default shipped twice"
done < <(find fs/etc/xdg -type f 2>/dev/null | sort)

if [ -f fs/etc/xdg/kdeglobals ]; then
	keys=$(awk '
		/^[[:space:]]*#/ || /^[[:space:]]*$/ { next }
		/^\[/ { g = $0; next }
		{ k = $0; sub(/=.*/, "", k); print g " " k }' fs/etc/xdg/kdeglobals)
	[ "$keys" = "[KDE] LookAndFeelPackage" ] ||
		bad "fs/etc/xdg/kdeglobals holds $(printf '%s' "$keys" | tr '\n' ';'); it carries [KDE] LookAndFeelPackage alone"
fi

for start in "" "/etc/xdg"; do
	got=$(env -i HOME=/nonexistent PATH=/usr/bin:/bin ${start:+XDG_CONFIG_DIRS=$start} \
		bash --noprofile --norc -c '. fs/etc/profile.d/10-wayland.sh >/dev/null 2>&1; printf %s "${XDG_CONFIG_DIRS:-}"')
	[ "$got" = "/etc/xdg:/usr/share/kdos/xdg" ] ||
		bad "a login shell started with XDG_CONFIG_DIRS=${start:-unset} ends with '$got'"
done

if [ -n "$ROOT" ]; then
	for w in kxkbrc kdosrc kwinrc; do
		[ -e "$ROOT/etc/xdg/$w" ] &&
			bad "$ROOT/etc/xdg/$w is on the image; only the installer writes it, per machine"
	done
	while IFS= read -r f; do
		rel=${f#"$ROOT"/etc/xdg/}
		case $rel in
		*rc | kdeglobals) [ -e "fs/etc/xdg/$rel" ] ||
			echo "  note  $ROOT/etc/xdg/$rel comes from a package: a default /etc/xdg holds for every user" ;;
		esac
	done < <(find "$ROOT/etc/xdg" -maxdepth 1 -type f 2>/dev/null | sort)
fi

echo "xdgcheck: $n file(s) under fs/etc/xdg$([ -n "$ROOT" ] && echo ", and $ROOT/etc/xdg"); $([ $fail = 0 ] && echo clean || echo failing)"
exit $fail
