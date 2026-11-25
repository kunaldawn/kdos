#!/bin/sh
# session-common.sh — the Plasma session's bring-up, in one place.
#
# Sourced, never executed: by kdos-session-wrapper before Plasma starts, and
# by /usr/share/kdos/xdg/plasma-workspace/env/kdos.sh when startplasma reads
# its environment scripts, which is also the path of a startplasma-wayland run
# by hand. Every one of these blocks carries a trap that fails without a
# message when it is missed, and a second copy is a second place to lose one.
#
#   kdos_session_open      $BROWSER, which the login shell may not have set
#   kdos_session_xdg_dirs  XDG_CONFIG_DIRS: the distribution's defaults, and
#                          the overlays of a virtual machine and the live medium
#   kdos_session_live_data XDG_DATA_DIRS: the live medium's own desktop entries
#   kdos_session_boxes     the appbox warmup
#   kdos_session_fcitx5_profile
#                          a new account's input-method profile, whose first
#                          entry is the system keyboard layout
#
# The session bus, the audio stack and the per-user timers are not here: they
# are user services (/etc/kdos/user-services), started once per user by
# turnstile at the first login, whatever kind of login that is, and stopped at
# the last. The keyboard layout itself is not set here: kdos-settingsd serves
# it as locale1 from /etc/kdos/xkb.conf, and KWin and startplasma read it
# there; the fcitx5 profile only names the same layout.

# ONE ROAD TO A LINK, ON A PATH THAT MAY HAVE READ NO PROFILE.
# /etc/profile.d sets $BROWSER for a login shell, and startplasma hands the
# activation environment only what is set — so a session started any other way
# would hand every D-Bus-started program no browser, and every boxed link would
# go nowhere. Only fills a gap: a person who exported their own has said what
# they want.
kdos_session_open() {
	BROWSER="${BROWSER:-xdg-open}"
	export BROWSER
}

# THE DISTRIBUTION'S DEFAULTS ARE THE LAST CONFIG DIRECTORY, AND ALWAYS THERE.
# Every KDOS default for a KDE program — the look-and-feel, the fonts, kwinrc,
# powerdevilrc, startkderc, this very directory's plasma-workspace/env — is in
# /usr/share/kdos/xdg, after /etc/xdg so the machine's owner overrides it key
# by key. A session whose XDG_CONFIG_DIRS lacks it reads Breeze and upstream's
# defaults with nothing to say so, and one that lacks /etc/xdg loses every
# package's autostart entry, the polkit agent's among them. Both are appended
# when missing and nothing else moves, so the order a session already carries
# is kept: startplasma puts ~/.config/kdedefaults in front of whatever it is
# handed.
#
# TWO OVERLAYS GO IN FRONT, EACH ONLY WHERE IT APPLIES, and ahead of /etc/xdg
# because what they turn off is wrong for that machine whatever the image
# says. ~/.config still outranks both, so a person can turn either back on.
#
#   /usr/share/kdos/xdg-vm    in a virtual machine (`kdos in-vm`, the DMI
#                             vendor test of libkbase): no idle dimming,
#                             blanking or timed lock, which from outside the
#                             guest read as a hung one
#   /usr/share/kdos/xdg-live  on the live medium, whose root is an overlay
#                             filesystem: no file index of a home that is gone
#                             at the next boot, no timed lock and no wallet
#                             for an account with no password, no notice of
#                             an update nobody can keep, and the welcome tour
#                             on its live page
#
# Each is added once, so a second call — the wrapper's, then the environment
# script's — changes nothing.
kdos_session_xdg_dirs() {
	_xd=${XDG_CONFIG_DIRS:-/etc/xdg}
	case ":$_xd:" in
		*:/etc/xdg:*) ;;
		*) _xd=$_xd:/etc/xdg ;;
	esac
	case ":$_xd:" in
		*:/usr/share/kdos/xdg:*) ;;
		*) _xd=$_xd:/usr/share/kdos/xdg ;;
	esac
	case ":$_xd:" in
		*:/usr/share/kdos/xdg-vm:*) ;;
		*)
			if [ -d /usr/share/kdos/xdg-vm ] && \
			   command -v kdos >/dev/null 2>&1 && \
			   kdos in-vm >/dev/null 2>&1; then
				_xd=/usr/share/kdos/xdg-vm:$_xd
			fi
			;;
	esac
	case ":$_xd:" in
		*:/usr/share/kdos/xdg-live:*) ;;
		*)
			if [ -d /usr/share/kdos/xdg-live ] && kdos_session_live; then
				_xd=/usr/share/kdos/xdg-live:$_xd
			fi
			;;
	esac
	XDG_CONFIG_DIRS=$_xd
	export XDG_CONFIG_DIRS
	unset _xd
}

# THE LIVE MEDIUM'S DESKTOP ENTRIES GO IN FRONT OF THE IMAGE'S, THERE ALONE.
# /usr/share/kdos/data-live holds the entries the live session starts
# differently: Welcome Center's, whose Exec passes --live-environment, which
# the tour's live pages (Install, Keep Your Changes, the live wording of
# Privacy and Make It Yours) read; Welcome Center's kded module passes it at
# login, and a tour reopened from the launcher without it is an installed
# machine's. An entry of the same desktop id earlier in XDG_DATA_DIRS
# replaces the image's, so the directory goes first, on the live medium only.
# XDG_DATA_DIRS unset means the specification's /usr/local/share:/usr/share,
# which the list keeps behind it. Added once, like the config overlays. The
# argument replaces the directory's path.
kdos_session_live_data() {
	_dl=${1:-/usr/share/kdos/data-live}
	_dd=${XDG_DATA_DIRS:-/usr/local/share:/usr/share}
	case ":$_dd:" in
		*":$_dl:"*) ;;
		*)
			if [ -d "$_dl" ] && kdos_session_live; then
				XDG_DATA_DIRS=$_dl:$_dd
				export XDG_DATA_DIRS
			fi
			;;
	esac
	unset _dd _dl
}

# The live medium: the root is the overlay the initramfs builds over the
# image, which no installed system's root is — the test `kdos doctor` makes of
# the same mount table. /proc/mounts names the root's type in the third field.
kdos_session_live() {
	while read -r _dev _mnt _type _rest; do
		if [ "$_mnt" = / ] && [ "$_type" = overlay ]; then
			unset _dev _mnt _type _rest
			return 0
		fi
	done < /proc/mounts
	unset _dev _mnt _type _rest
	return 1
}

# Pre-create and start the boxes behind the pinned favourites while the
# desktop is coming up, one box per application, so the first click on each
# doesn't pay for container init. nice 10, not 19: at 19 the warmup loses every
# CPU slice to the starting desktop and stays mid-init for minutes, and a
# launch that lands in that window has to wait for it to finish. A second
# warmup while one runs is a no-op (kdos-appbox takes a lock), so every login
# may ask.
#
# Giving the idle ones back is the `box-gc` row of
# /etc/kdos/user-services/timers.d, under the user's own services, and the
# session's end (plasma-workspace/shutdown/kdos.sh).
kdos_session_boxes() {
	command -v kdos-appbox >/dev/null 2>&1 || return 0
	(nice -n 10 kdos-appbox warmup >/dev/null 2>&1 &)
}

# A NEW ACCOUNT'S FCITX5 PROFILE NAMES THE SYSTEM LAYOUT, NEVER A FIXED ONE.
# fcitx5, which KWin starts as the input method, takes its group's Default
# Layout as its inactive state. With its Wayland module's 'Allow Overriding
# System XKB Settings' on, it also writes that layout into ~/.config/kxkbrc as
# LayoutList, which outranks /etc/xdg/kxkbrc; /etc/xdg/fcitx5/conf/wayland.conf
# turns the option off, and the Input Method page can turn it back on. A
# profile naming any layout but the machine's would then put that layout
# under the lock screen, which checks a password typed on the wrong keys.
# So the profile is written here, before
# KWin starts, from /etc/kdos/xkb.conf (the argument replaces that path): the
# first entry of XKBLAYOUT's and of XKBVARIANT's comma lists, and when the
# layout is empty, the one locale1 serves, which follows the console keymap;
# `us` when neither names one. A value outside xkb.conf's character set
# (letters, digits, `_+:.-`) is not used. The group is the four input methods
# the image carries: the layout, then pinyin, anthy and hangul.
#
# An existing profile is never touched, so the Input Method page's choices,
# and a layout a person picked, stand. The file is written whole under a
# temporary name and renamed, so a login cut short leaves no half profile for
# the next one to keep.
kdos_session_fcitx5_profile() {
	_fx_dir=${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5
	[ -n "$HOME" ] || [ -n "${XDG_CONFIG_HOME:-}" ] || return 0
	[ -e "$_fx_dir/profile" ] && { unset _fx_dir; return 0; }
	_fx_l=
	_fx_v=
	if [ -r "${1:-/etc/kdos/xkb.conf}" ]; then
		while IFS= read -r _fx_line || [ -n "$_fx_line" ]; do
			case $_fx_line in
				XKBLAYOUT=*) _fx_l=${_fx_line#XKBLAYOUT=} ;;
				XKBVARIANT=*) _fx_v=${_fx_line#XKBVARIANT=} ;;
			esac
		done < "${1:-/etc/kdos/xkb.conf}"
	fi
	if [ -z "$_fx_l" ] && command -v dbus-send >/dev/null 2>&1; then
		_fx_l=$(kdos_session_locale1 X11Layout)
		_fx_v=$(kdos_session_locale1 X11Variant)
	fi
	_fx_l=$(kdos_session_xkb_first "$_fx_l")
	_fx_v=$(kdos_session_xkb_first "$_fx_v")
	[ -n "$_fx_l" ] || { _fx_l=us; _fx_v=; }
	[ -n "$_fx_v" ] && _fx_l=$_fx_l-$_fx_v
	if mkdir -p "$_fx_dir" 2>/dev/null && printf '%s\n' \
		'[Groups/0]' '# Group Name' 'Name=Default' '# Layout' \
		"Default Layout=$_fx_l" '# Default Input Method' \
		"DefaultIM=keyboard-$_fx_l" '' \
		'[Groups/0/Items/0]' '# Name' "Name=keyboard-$_fx_l" '# Layout' 'Layout=' '' \
		'[Groups/0/Items/1]' '# Name' 'Name=pinyin' '# Layout' 'Layout=' '' \
		'[Groups/0/Items/2]' '# Name' 'Name=anthy' '# Layout' 'Layout=' '' \
		'[Groups/0/Items/3]' '# Name' 'Name=hangul' '# Layout' 'Layout=' '' \
		'[GroupOrder]' '0=Default' > "$_fx_dir/.profile.kdos.$$" 2>/dev/null; then
		mv -f "$_fx_dir/.profile.kdos.$$" "$_fx_dir/profile" 2>/dev/null || \
			rm -f "$_fx_dir/.profile.kdos.$$"
	else
		rm -f "$_fx_dir/.profile.kdos.$$" 2>/dev/null
	fi
	unset _fx_dir _fx_l _fx_v _fx_line
}

# The first entry of an XKB comma list, quotes removed, or nothing when it
# holds a character xkb.conf does not allow.
kdos_session_xkb_first() {
	_xf=$1
	case $_xf in
		\"*\") _xf=${_xf#\"}; _xf=${_xf%\"} ;;
		\'*\') _xf=${_xf#\'}; _xf=${_xf%\'} ;;
	esac
	_xf=${_xf%%,*}
	case $_xf in
		*[!A-Za-z0-9_+:.-]*) _xf= ;;
	esac
	printf '%s' "$_xf"
	unset _xf
}

# One string property of locale1 on the system bus, or nothing.
kdos_session_locale1() {
	_lv=$(dbus-send --system --print-reply=literal --reply-timeout=2000 \
		--dest=org.freedesktop.locale1 /org/freedesktop/locale1 \
		org.freedesktop.DBus.Properties.Get \
		string:org.freedesktop.locale1 "string:$1" 2>/dev/null) || _lv=
	case $_lv in
		*variant*) _lv=${_lv#*variant} ;;
		*) _lv= ;;
	esac
	_lv=${_lv#"${_lv%%[! 	]*}"}
	printf '%s' "${_lv%%[ 	]*}"
	unset _lv
}
