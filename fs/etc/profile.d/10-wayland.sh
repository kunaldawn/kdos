# Session environment for every login shell.
# Sourced by /etc/profile on every login: a console, the serial line, ssh, and
# the login shell kdos-session-wrapper re-executes itself in before it starts
# Plasma. So every export here reaches the desktop and everything it starts,
# and a name that should differ between the desktop and a console is only ever
# filled in here, never forced.

uid=$(id -u)
# A bare console (serial getty, rescue shell) can land here with HOME unset;
# every $HOME-derived export below would then silently become "//..." and
# follow the session around through su. Read the home field out of /etc/passwd
# instead; an empty answer here lands the desktop user on /root. $uid is
# numeric, so the match is on field 3, and only the first line counts: two
# lines here would make every export below multi-line.
if [ -z "$HOME" ]; then
	HOME=$(grep "^[^:]*:[^:]*:$uid:" /etc/passwd 2>/dev/null | head -1 | cut -d: -f6)
	[ -n "$HOME" ] || HOME=/root
	export HOME
fi
# THE RUNTIME DIRECTORY IS NAMED ONLY WHEN IT EXISTS. turnstile makes
# /run/user/<uid> at a user's first login and removes it after the last, and
# PAM exports the variable into every login it registers. A shell no PAM session
# started (ssh, su without -l) is pointed at the directory of a login that is
# still open, or at none: a variable naming a directory that is not there sends
# every program that uses it to fail on its first socket.
if [ -z "$XDG_RUNTIME_DIR" ] && [ -d "/run/user/$uid" ]; then
	export XDG_RUNTIME_DIR="/run/user/$uid"
fi

# THE DESKTOP'S NAMES, FOR A SHELL NO SESSION STARTED. Inside Plasma, SDDM sets
# XDG_CURRENT_DESKTOP from the session's DesktopNames and startplasma sets both
# again; a console or ssh shell gets the same names here, filled in and never
# forced. kde-mimeapps.list, where every KDOS default application is named, is
# read only by a program whose XDG_CURRENT_DESKTOP names KDE: without it a file
# opened from a console opens in whatever handler the plain lists name, not in
# the application the desktop opens it in. The prefix names the menu the
# desktop's own launcher reads, plasma-applications.menu.
export XDG_CURRENT_DESKTOP="${XDG_CURRENT_DESKTOP:-KDE}"
export XDG_MENU_PREFIX="${XDG_MENU_PREFIX:-plasma-}"

# The per-user session bus lives at a fixed runtime path, the one every box
# binds; turnstile starts it and PAM exports its address into each login it
# registers. A shell no PAM session started is pointed at it when it is up.
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ] && [ -n "$XDG_RUNTIME_DIR" ] && \
   [ -S "$XDG_RUNTIME_DIR/bus" ]; then
	export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
fi

# Where launchers look for .desktop files. XDG_DATA_HOME must NOT be repeated
# inside XDG_DATA_DIRS — launchers scan both, and every app shows up twice.
export XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"
export XDG_DATA_DIRS="${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
# KDOS'S DEFAULTS COME AFTER /etc/xdg, so a file the machine's owner puts in
# /etc/xdg wins over the one the image ships, and an update never collides with
# an edited /etc file. Appended when it is missing rather than set, so a list a
# session already carries keeps its order: startplasma puts
# ~/.config/kdedefaults in front of whatever it is handed.
case ":${XDG_CONFIG_DIRS:-/etc/xdg}:" in
	*:/usr/share/kdos/xdg:*) ;;
	*) export XDG_CONFIG_DIRS="${XDG_CONFIG_DIRS:-/etc/xdg}:/usr/share/kdos/xdg" ;;
esac

# kdos-fetch-app also drops a CLI wrapper per alien app here, so `gimp` works
# from any shell, not just from the launcher.
case ":$PATH:" in
	*":$HOME/.local/bin:"*) ;;
	*) export PATH="$HOME/.local/bin:$PATH" ;;
esac
# Distrobox passes the host PATH into the appbox, and debian puts its games
# under /usr/games — without this every game launcher dies on "not found".
case ":$PATH:" in
	*":/usr/games:"*) ;;
	*) export PATH="$PATH:/usr/games" ;;
esac

# Toolkit back ends. GTK and Qt are built with their Wayland and X11 back ends,
# and Wayland comes first; SDL is built with Wayland alone, so its driver
# names no fallback; Tk and Motif have only X11 and run under Xwayland.
# Qt takes a list and falls back to xcb (Xwayland) only when the Wayland
# plugin cannot start, which is a Qt 5 application built without one; a name
# on its own here would leave that application not starting at all. GDK_BACKEND
# is left unset: GDK already tries Wayland first, and a value here would
# override the GDK_BACKEND=x11 an X11-only application sets for itself.
export QT_QPA_PLATFORM="wayland;xcb"
export MOZ_ENABLE_WAYLAND=1
export SDL_VIDEODRIVER=wayland
export CLUTTER_BACKEND=wayland

# Native GTK and Qt applications on the host. GTK_USE_PORTAL=1 sends GTK 3's
# native file chooser and print dialog to the portals, so a GTK 3 application
# that uses them opens the KDE file dialog, and Firefox's file picker reads it
# too; GTK 4 uses the portals without it, and a program that builds GTK's own
# file dialog widget keeps GTK's. GTK 3 then reads its theme, fonts and the
# rest from the Settings portal, but loads colorreload-gtk-module, its only
# way to repaint on a colour change, from org.gtk.Settings: gtk3 is patched to
# open that service beside the portal, and kde-gtk-config to serve it although
# kded inherits the variable. Qt takes one platform theme name here, not a
# list, and each Qt major version searches only its own plugin directory: a Qt 6 application finds plasma-integration's
# `kde` theme and the colours of kdeglobals; a Qt 5 application finds the qt5ct
# plugin under `kde` (the qt5ct port adds the key) and reads the ~/.config/qt5ct
# files `kdos theme` writes from the KDE colour scheme. Without it, a Qt 5
# application paints Qt's built-in palette under every KDOS theme.
export GTK_USE_PORTAL=1
export QT_QPA_PLATFORMTHEME=kde
