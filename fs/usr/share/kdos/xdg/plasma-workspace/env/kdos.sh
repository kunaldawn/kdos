# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   The environment every Plasma session starts with.
#
# Sourced by startplasma, which runs every plasma-workspace/env/*.sh it finds
# in the XDG config directories — the user's ~/.config last, so a person's own
# script there overrides this one — in one /bin/sh, before plasma_session, and
# takes into the session every variable they leave SET. A variable unset here
# is not unset in the session: startplasma imports what the scripts export and
# removes nothing. Anything the session must not carry is therefore cleared
# before startplasma runs, by kdos-session-wrapper.
#
# It is found through XDG_CONFIG_DIRS, so it runs in every session whose list
# holds /usr/share/kdos/xdg: the SDDM session, which kdos-session-wrapper
# starts, and a startplasma-wayland run by hand from a login shell, which
# /etc/profile.d/10-wayland.sh gave the list. It settles the same names both
# times.
# ---------------------------------

if [ -r /usr/local/lib/kdos/session-common.sh ]; then
	. /usr/local/lib/kdos/session-common.sh
	kdos_session_xdg_dirs
	kdos_session_open
fi

# A passphrase asked for with no terminal to ask on — ssh-add from an
# autostart entry, git or ssh run from an application — is asked in
# ksshaskpass's dialog, whose answer the session's secret store can keep.
# SSH_ASKPASS_REQUIRE stays unset, so ssh and ssh-add run in Konsole still
# ask on its terminal: OpenSSH runs the dialog only when it has none. git's
# own prompts — an HTTPS user name and password — are the exception: git
# prefers SSH_ASKPASS to the terminal when GIT_ASKPASS and core.askPass are
# unset, so they come up in the dialog even in Konsole.
if [ -z "${SSH_ASKPASS:-}" ] && [ -x /usr/bin/ksshaskpass ]; then
	SSH_ASKPASS=/usr/bin/ksshaskpass
	export SSH_ASKPASS
fi

# Qt Quick draws its distance-field text, which Plasma's labels use, with
# subpixel antialiasing unless this says otherwise: fontconfig's rgba=none and
# kdeglobals' XftSubPixel never reach it, and every panel label, notification
# and the KSplash footer would carry coloured fringes that no widget does.
QSG_DISTANCEFIELD_ANTIALIASING=gray
export QSG_DISTANCEFIELD_ANTIALIASING

# The desktop every program asks about, for the mimeapps list
# (kde-mimeapps.list), the portal (kde-portals.conf) and OnlyShowIn=KDE.
# startplasma sets the same name after the scripts; set here, the scripts
# sourced after this one see it too.
XDG_CURRENT_DESKTOP=KDE
export XDG_CURRENT_DESKTOP
