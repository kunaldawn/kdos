# Wayland session environment.
# Sourced by /etc/profile on every login. Sets up the runtime directory
# (which would normally be created by elogind/systemd-logind) and the env
# vars Wayland-aware toolkits look for.

uid=$(id -u)
# A bare console (serial getty, rescue shell) can land here with HOME unset;
# every $HOME-derived export below would then silently become "//..." and
# follow the session around through su. Derive it from passwd instead.
if [ -z "$HOME" ]; then
	HOME=$(getent passwd "$uid" | cut -d: -f6)
	[ -n "$HOME" ] && export HOME || HOME=/root
fi
if [ -z "$XDG_RUNTIME_DIR" ]; then
	export XDG_RUNTIME_DIR="/run/user/$uid"
fi
if [ ! -d "$XDG_RUNTIME_DIR" ]; then
	mkdir -p "$XDG_RUNTIME_DIR"
	chmod 0700 "$XDG_RUNTIME_DIR"
	chown "$uid:$uid" "$XDG_RUNTIME_DIR" 2>/dev/null || true
fi

export XDG_SESSION_TYPE=wayland
# NOT OVER A SESSION THAT ALREADY SAID WHICH ONE IT IS. The console session
# exports `KDOS-Console:KDOS` before it starts anything and the greeter exports
# `KDOS` for the graphical one, and that name is what selects
# `kdos-console-mimeapps.list` or `kdos-mimeapps.list` — so a profile script
# overwriting it would make every shell inside one desktop resolve a file's
# handler as though it were running under the other.
#
# AND A LOGIN THAT NO SESSION STARTED IS NOT THE COMPOSITOR'S. `Ctrl+Alt+F2`, a
# serial console and an ssh login all reach here with the variable unset and no
# display of any kind; naming `KDOS` there sent every link and every document to
# `kdos-mimeapps.list`, whose every row is a Wayland client with nothing to
# connect to. The console's rows are the terminal ones, which are the only ones
# that can run on a bare virtual terminal — so that is the name it gets.
if [ -z "$XDG_CURRENT_DESKTOP" ]; then
	if [ -n "$WAYLAND_DISPLAY" ]; then
		export XDG_CURRENT_DESKTOP=KDOS
	else
		export XDG_CURRENT_DESKTOP="KDOS-Console:KDOS"
	fi
fi

# The per-user session bus lives at a fixed runtime path (started by
# kdos-desktop; the same path is visible inside the appbox). Point shells
# that didn't inherit the session env (ssh, tty2) at it when it's up.
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ] && [ -S "$XDG_RUNTIME_DIR/bus" ]; then
	export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
fi

# Where launchers look for .desktop files. XDG_DATA_HOME must NOT be repeated
# inside XDG_DATA_DIRS — launchers scan both, and every app shows up twice.
export XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"
export XDG_DATA_DIRS="${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"

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
# Cursor theme: kdos-comp, Wayland clients, and Qt all read these; GTK apps
# in the appbox read ~/.config/gtk-3.0/settings.ini (seeded in skel).
export XCURSOR_THEME=KDOS-cursors
export XCURSOR_SIZE=24

export QT_QPA_PLATFORM=wayland
export GDK_BACKEND=wayland
export MOZ_ENABLE_WAYLAND=1
export _JAVA_AWT_WM_NONREPARENTING=1
export SDL_VIDEODRIVER=wayland
export CLUTTER_BACKEND=wayland
