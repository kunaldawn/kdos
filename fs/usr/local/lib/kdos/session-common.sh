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
#   kdos_session_boxes     the appbox warmup
#
# The session bus, the audio stack and the per-user timers are not here: they
# are user services (/etc/kdos/user-services), started once per user by
# turnstile at the first login, whatever kind of login that is, and stopped at
# the last. The keyboard layout is not here either: kdos-settingsd serves it as
# locale1 from /etc/kdos/xkb.conf, and KWin and startplasma read it there.

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
